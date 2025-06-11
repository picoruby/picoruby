/*
 * picoruby-psg/src/psg.c
 */

#include "../include/psg.h"

#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/class.h"
#include "mruby/hash.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

static inline void
__breakpoint(void)
{
  __asm volatile ("bkpt #0");
}

#ifndef PSG_COMPILER_BARRIER
#  if defined(__GNUC__) || defined(__clang__)
#    define PSG_COMPILER_BARRIER() __asm volatile("" ::: "memory")
#  elif __STDC_VERSION__ >= 201112L
#    include <stdatomic.h>
#    define PSG_COMPILER_BARRIER() atomic_thread_fence(memory_order_seq_cst)
#  else
#    define PSG_COMPILER_BARRIER() /* nothing (best effort) */
#  endif
#endif

#define MAX_LFO_DEPTH 127  // 0..127 cent
#define MAX_LFO_RATE  255  // 0..255 (0.1 Hz steps, 0-25.5 Hz)

/*
                          | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
R0  TR A tone period      |             LSB (0-255)               |
R1  TR A tone period      |-------------------|     MSB (0-15)    |
R2  TR B tone period      |             LSB (0-255)               |
R3  TR A tone period      |-------------------|     MSB (0-15)    |
R4  TR C tone period      |             LSB (0-255)               |
R5  TR A tone period      |-------------------|     MSB (0-15)    |
R6  Noise period (0-31)   |--------------|        5 bit NP        |
R7  Mixer (0-63)          |(IOB,IOA)| C  | B  | A  | C  | B  | A  |
     0: on, 1: off                  ^----noise-----^-----tone-----^
R8  TR A volume (0-15)    |--------------| M  | L3 | L2 | L1 | L0 | # If M=1,
R9  TR B volume (0-15)    |--------------| M  | L3 | L2 | L1 | L0 | # volume value is ignored
R10 TR C volume (0-15)    |--------------| M  | L3 | L2 | L1 | L0 | # and envelope is used instead
R11 Envelope period       |             LSB (0-255)               |
R12 Envelope period MSB   |             MSB (0-255)               |
R13 Envelope shape (0-15) |-------------------| E3 | E2 | E1 | E0 |
     E3: 0=continue,  1=stop          E2: 0=attack, 1=release
     E1: 0=alternate, 1=sawtooth      E0: 0=hold,   1=repeat
     R13: B3 B2 B1 B0
           0  0  x  x  _＼___________

           0  1  x  x  _／___________

           1  0  0  0  _＼＼＼＼＼＼＼

           1  0  0  1  _＼___________

           1  0  1  0  _＼／＼／＼／＼

           1  0  1  1  _＼￣￣￣￣￣￣

           1  1  0  0  _／／／／／／／

           1  1  0  1  _／￣￣￣￣￣￣

           1  1  1  0  _／＼／＼／＼／

           1  1  1  1  _／___________
                        ^^
                        Period
*/

typedef struct {
  uint16_t tone_period[3];   // R0–5  (12-bit)
  uint8_t  noise_period;     // R6
  uint8_t  mixer;            // R7
  uint8_t  volume[3];        // R8–10
  uint16_t envelope_period;  // R11–12
  uint8_t  envelope_shape;   // R13
} psg_regs_t;

typedef enum {
  PSG_TIMBRE_SQUARE = 0,  // square wave
  PSG_TIMBRE_TRIANGLE,    // triangle wave
  PSG_TIMBRE_SAWTOOTH,    // sawtooth wave
  PSG_TIMBRE_INVSAWTOOTH, // inverted sawtooth wave
} psg_timbre_t;

typedef struct {
  psg_regs_t r;
  uint32_t tone_inc[3];      // 32.32 fixed-point number
  uint32_t tone_phase[3];
  uint32_t noise_shift;
  uint32_t noise_cnt;
  uint32_t env_cnt;          // 24-bit counter: 1step = 1 audio sample
  // envelope state machine
  uint8_t  env_level;     // 0..15
  uint8_t  env_dir;       // 0=down, 1=up
  bool     env_running;   // false -> stop
  // Masked bit for performance
  uint8_t  env_continue;
  uint8_t  env_attack;
  uint8_t  env_alternate;
  uint8_t  env_hold;
  // LFO
  uint16_t lfo_phase[3];   /* 0..65535 (wrap) */
  uint16_t lfo_inc[3];     /* Δphase per 1 ms tick */
  uint8_t  lfo_depth[3];   /* depth in cent (0..127) */
  // Mute
  uint8_t  mute_mask;      /* bit0=A bit1=B bit2=C */
  // pan
  uint8_t pan[3];
  // tone type
  psg_timbre_t timbre[3];
} psg_t;

static psg_t psg;

static psg_ringbuf_t rb = {
  .head = 0,
  .tail = 0,
  .buf = NULL,  // will be allocated later
};

static inline uint32_t
calc_inc(uint16_t period)
{
  if (!period) return 0;
  // f = clk / (32*(N)) -> inc = f / Fs (32.32)
  uint32_t f = CHIP_CLOCK / (32u * period);
  return ((uint64_t)f << 32) / SAMPLE_RATE;
}

static inline void
update_tone_inc(int tr)
{
  psg.tone_inc[tr] = calc_inc(psg.r.tone_period[tr]);
}

// AY compatibile registors
static void
PSG_write_reg(uint8_t reg, uint8_t val)
{
  psg_cs_token_t tok = PSG_enter_critical();

  switch (reg) {
    /* ---- Tone period ---- */
    case 0:   /* tr A LSB */
    case 1:   /* tr A MSB */
      psg.r.tone_period[0] &= reg ? 0x00FF : 0x0F00;
      psg.r.tone_period[0] |= reg ? ((val & 0x0F) << 8) : val;
      if (reg == 1) update_tone_inc(0);
      break;
    case 2:   /* tr B LSB */
    case 3:   /* tr B MSB */
      psg.r.tone_period[1] &= reg & 1 ? 0x00FF : 0x0F00;
      psg.r.tone_period[1] |= reg & 1 ? ((val & 0x0F) << 8) : val;
      if (reg == 3) update_tone_inc(1);
      break;
    case 4:   /* tr C LSB */
    case 5:   /* tr C MSB */
      psg.r.tone_period[2] &= reg & 1 ? 0x00FF : 0x0F00;
      psg.r.tone_period[2] |= reg & 1 ? ((val & 0x0F) << 8) : val;
      if (reg == 5) update_tone_inc(2);
      break;
    /* ---- Noise ---- */
    case 6:
      psg.r.noise_period = val & 0x1F;
      break;
    /* ---- Mixer ---- */
    case 7:
      psg.r.mixer = val & 0x3F; // 0b00111111
      break;
    /* ---- Volume ---- */
    case 8:  case 9:  case 10:
      psg.r.volume[reg - 8] = val & 0x1F; // 0b00011111
      break;
    /* ---- Envelope ---- */
    case 11:          /* period LSB */
      psg.r.envelope_period = (psg.r.envelope_period & 0xFF00) | val;
      break;
    case 12:          /* period MSB */
      psg.r.envelope_period = (psg.r.envelope_period & 0x00FF) | (val << 8);
      break;
    case 13:          /* shape */
      psg.r.envelope_shape = val & 0x0F;
      // Reset state machine on shape setting
      psg.env_continue  = (val >> 3) & 1;
      psg.env_attack    = (val >> 2) & 1;
      psg.env_alternate = (val >> 1) & 1;
      psg.env_hold      =  val       & 1;
      psg.env_dir       = psg.env_attack;
      psg.env_level     = psg.env_attack ? 0 : 15;
      psg.env_running   = true;
      psg.env_cnt       = 0;
      break;
    default:
      break;
  }
  PSG_exit_critical(tok);
}


void
PSG_process_packet(const psg_packet_t *pkt)
{
  switch (pkt->op) {
    case PSG_PKT_REG_WRITE:
      PSG_write_reg(pkt->reg, pkt->val);
      break;
    case PSG_PKT_LFO_SET: {
      uint8_t tr    = pkt->reg & 0x03;
      uint8_t depth = pkt->val;   /* cent */
      uint8_t rate  = pkt->arg;   /* 0.1 Hz */
      if (depth > MAX_LFO_DEPTH || rate > MAX_LFO_RATE) break;
      psg_cs_token_t t = PSG_enter_critical();
      psg.lfo_depth[tr] = depth;
      /* Δphase per 1 ms = rate(0.1 Hz) × 65536 / 1000 */
      psg.lfo_inc[tr]   = ((uint32_t)rate * 65536u) / 1000;
      PSG_exit_critical(t);
      break;
    }
    case PSG_PKT_CH_MUTE: {
      uint8_t tr   = pkt->reg & 0x03;
      uint8_t flag = pkt->val;
      psg_cs_token_t t = PSG_enter_critical();
      if (flag) psg.mute_mask |=  (1u << tr);
      else psg.mute_mask &= ~(1u << tr);
      PSG_exit_critical(t);
      break;
    }
    case PSG_PKT_PAN_SET: {
      uint8_t tr  = pkt->reg & 0x03;
      uint8_t bal = pkt->val; // 0..15
      psg_cs_token_t t = PSG_enter_critical();
      psg.pan[tr] = bal & 0x0F;   /* 4bit keep */
      PSG_exit_critical(t);
      break;
    }
    case PSG_PKT_TIMBRE_SET: {
      uint8_t tr = pkt->reg & 0x03;
      uint8_t type = pkt->val; // 0=square, 1=triangle
      psg_cs_token_t t = PSG_enter_critical();
      psg.timbre[tr] = (psg_timbre_t)type;
      PSG_exit_critical(t);
      break;
    }
    default: // ignore?
      break;
  }
}

/* producer (core-0) ------------------------------------------------- */
static bool
PSG_rb_push(const psg_packet_t *p)
{
  uint16_t next = (rb.head + 1) & PSG_PACKET_QUEUE_MASK;
  if (next == rb.tail) return false;  // full -> drop
  rb.buf[rb.head] = *p;
  PSG_COMPILER_BARRIER();
  rb.head = next;
  return true;
}

/* consumer (core-1) ------------------------------------------------- */
bool
PSG_rb_peek(psg_packet_t *out)  // does not consume
{
  if (rb.tail == rb.head) return false;
  *out = rb.buf[rb.tail];
  return true;
}

void
PSG_rb_pop(void)  // consumes one slot
{
  rb.tail = (rb.tail + 1) & PSG_PACKET_QUEUE_MASK;
  PSG_COMPILER_BARRIER();
}


// volume table: AY dB curve -> 12-bit linear
static const uint16_t vol_tab[16] = {
  0,     //  -inf dB
  33,    // −24.1 dB
  46,    // −19.1
  65,    // −15.0
  92,    // −12.0
  132,   //  −9.5
  186,   //  −7.5
  263,   //  −6.0
  372,   //  −4.8
  528,   //  −3.8
  747,   //  −3.0
  1059,  //  −2.2
  1499,  //  −1.6
  2123,  //  −1.0
  3006,  //  −0.6
  4095   //   0.0 (full)
};

// Pan table: 1 = L-only,  8 = center, 15 = R-only. 0 should not be used.
static const uint16_t pan_tab_l[16] = {
  // 0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,   15
  4095, 4095, 4069, 3992, 3865, 3689, 3467, 3201, 2895, 2553, 2178, 1776, 1352,  911,  458,    0
};
static const uint16_t pan_tab_r[16] = {
  // 0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,   15
     0,    0,  458,  911, 1352, 1776, 2178, 2553, 2895, 3201, 3467, 3689, 3865, 3992, 4069, 4095
};

static inline void
update_envelope(void)
{
  if (!psg.env_running || !psg.r.envelope_period) return;

  uint32_t target = ((uint32_t)psg.r.envelope_period + 1) << 8; /* 256× */
  if (++psg.env_cnt < target) return;
  psg.env_cnt = 0;

  /* 4µs - 1s (from datasheet): 1 period = 1 step (0‒15) */
  if (psg.env_dir) {                // up
    if (psg.env_level < 15) {
      ++psg.env_level;
      return;
    }
  } else {                          // down
    if (psg.env_level > 0) {
      --psg.env_level;
      return;
    }
  }

  // Reached the end of the envelope
  if (!psg.env_continue) {          // C = 0   -> Stop
    psg.env_running = false;
    return;
  }

  if (psg.env_hold) {               // C=1, H=1 -> No repeat
    psg.env_running = false;
    return;
  }

  if (psg.env_alternate)            // Reverse direction
    psg.env_dir ^= 1;

  // Attack bit is valid only for the first cycle
  if (!psg.env_alternate)
    psg.env_dir = psg.env_attack; // Sawtooth

  // In order to be faithful to the datasheet, advance one step after reversal
  if (psg.env_dir) {
    psg.env_level = 0;
  } else {
    psg.env_level = 15;
  }
}

void
PSG_tick_1ms(void)
{
  // Advance LFO phase (called from 1 kHz system timer)
  for (int tr = 0; tr < 3; ++tr) {
    psg.lfo_phase[tr] += psg.lfo_inc[tr];
  }
}

const psg_output_api_t *psg_drv = NULL;

bool
PSG_audio_cb(void)
{
  update_envelope();

  // noise LFSR (17-bit)
  psg.noise_cnt++;
  if (psg.noise_cnt >= (psg.r.noise_period + 1)) {
    uint32_t fb = ((psg.noise_shift ^ (psg.noise_shift >> 3)) & 1);
    psg.noise_shift = (psg.noise_shift >> 1) | (fb << 16);
    psg.noise_cnt = 0;
  }
  uint8_t noise_bit = psg.noise_shift & 1;

  // mix
  uint32_t mix_l = 0, mix_r = 0;

  for (int tr = 0; tr < 3; ++tr) {
    // phase
    if (psg.tone_inc[tr]) {
      /* Vibrato: ±depth cent  -> multiplicative factor ~= 2^(cent/1200) */
      int8_t depth = (int8_t)psg.lfo_depth[tr];          /* signed */
      uint16_t ph  = psg.lfo_phase[tr];
      /* simple triangle LFO: 0-32767-0-… */
      int16_t tri = (ph < 32768) ? ph : (65535 - ph);    /* 0-32767 */
      int32_t cent = (depth * tri) >> 15;                /* −depth..+depth */
      /* ln(2)/1200 ≒ 0.0005775  -> use 16.16 fixed ->> 38 */
      int32_t frac = (cent * 38) >> 8;                   /* ~= log2 factor */
      uint32_t inc = psg.tone_inc[tr] + ((psg.tone_inc[tr] * frac) >> 16);  /* FM */
      psg.tone_phase[tr] += inc;
    }

    uint32_t tone_amp;
    switch (psg.timbre[tr]) {
      case PSG_TIMBRE_TRIANGLE: {
        bool second = (psg.tone_phase[tr] & 0x80000000);
        uint32_t ramp = psg.tone_phase[tr] >> 20;
        tone_amp = second ? (4095 - ramp) * 2 : ramp * 2; // 0->4095->0
        break;
      }
      case PSG_TIMBRE_SAWTOOTH: {
        tone_amp = psg.tone_phase[tr] >> 20;  // 0->4095
        break;
      }
      case PSG_TIMBRE_INVSAWTOOTH: {
        tone_amp = 4095 - (psg.tone_phase[tr] >> 20);  // 4095->0
        break;
      }
      default: // PSG_TIMBRE_SQUARE & fallback
        tone_amp = (psg.tone_phase[tr] >> 31) ? 4095 : 0;
        break;
    }

    // noise mixing
    bool use_tone  = !(psg.r.mixer & (1 << tr));
    bool use_noise = !(psg.r.mixer & (1 << (tr + 3)));
    uint32_t active_amp = 0;
    if (use_tone)  active_amp += tone_amp;
    if (use_noise && noise_bit) active_amp = 4095;

    // Track mute
    if (psg.mute_mask & (1u << tr)) continue;
    if (active_amp == 0) continue;

    // volume: bit4 = envelope
    uint8_t vol = psg.r.volume[tr];
    if (vol & 0x10) vol = psg.env_level;
    vol &= 0x0F;

    uint32_t gain = vol_tab[vol];  // 0..4095
    uint32_t amp = (active_amp * gain) >> 12;

    // pan
    uint8_t bal = psg.pan[tr];          // 1..15
    mix_l += (amp * pan_tab_l[bal]) >> 12; // 0..4095
    mix_r += (amp * pan_tab_r[bal]) >> 12; // 0..4095
  }
  if (mix_l > MAX_SAMPLE_WIDTH) mix_l = MAX_SAMPLE_WIDTH;
  if (mix_r > MAX_SAMPLE_WIDTH) mix_r = MAX_SAMPLE_WIDTH;

  if (psg_drv && psg_drv->write) {
    psg_drv->write(mix_l, mix_r);
  }
  return true;
}

// mruby methods

static mrb_value
mrb_driver_send_reg(mrb_state *mrb, mrb_value klass)
{
  mrb_int reg, val;
  mrb_int tick_delay = 0;
  /* Accept 2 or 3 positional arguments:
     (reg, val)             -> immediate (delay=0)
     (reg, val, tick_delay) -> delayed              */
  mrb_get_args(mrb, "ii|i", &reg, &val, &tick_delay);
  psg_packet_t p = {
    .tick = (uint32_t)tick_delay,
    .op   = PSG_PKT_REG_WRITE,
    .reg  = (uint8_t)reg,
    .val  = (uint8_t)val,
  };
  return PSG_rb_push(&p) ? mrb_true_value() : mrb_false_value();
}

static void
reset_psg(mrb_state *mrb)
{
  if (rb.buf) {
    mrb_free(mrb, rb.buf);
    rb.buf = NULL;
  }
  if (rb.buf == NULL) {
    rb.buf = mrb_malloc(mrb, sizeof(psg_packet_t) * PSG_PACKET_QUEUE_LEN);
  }
  psg_cs_token_t t = PSG_enter_critical();
  memset(&psg, 0, sizeof(psg));
  psg.r.volume[0] = psg.r.volume[1] = psg.r.volume[2] = 15; // max volume. no envelope
  psg.r.mixer = 0x38; // all noise off, all tone on
  psg.r.envelope_period = 0x0B; // 0b00000000_00001011
  psg.pan[0] = psg.pan[1] = psg.pan[2] = 8; // center pan
  PSG_exit_critical(t);
}

static mrb_value
mrb_driver_s_select_pwm(mrb_state *mrb, mrb_value klass)
{
  psg_drv = &psg_drv_pwm;
  mrb_int left, right;
  mrb_get_args(mrb, "ii", &left, &right);
  mrb_warn(mrb, "PSG: PWM left=%d, right=%d\n", left, right);
  reset_psg(mrb);
  PSG_tick_start_core1((uint8_t)left, (uint8_t)right, 0, 0);
  return mrb_nil_value();
}

static mrb_value
mrb_driver_s_select_mcp492x(mrb_state *mrb, mrb_value klass)
{
  mrb_int dac, copi, sck, cs, ldac;
  mrb_get_args(mrb, "iiiii", &dac, &copi, &sck, &cs, &ldac);
  if (dac == 1) {
    psg_drv = &psg_drv_mcp4921;
  } else if (dac == 2) {
    psg_drv = &psg_drv_mcp4922;
  } else {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "Invalid DAC number: %d (1 or 2 expected)", dac);
  }
  reset_psg(mrb);
  PSG_tick_start_core1((uint8_t)copi, (uint8_t)sck, (uint8_t)cs, (uint8_t)ldac);
  return mrb_nil_value();
}

//static mrb_value
//mrb_driver_s_select_usbaudio(mrb_state *mrb, mrb_value klass)
//{
//  reset_psg(mrb);
//  PSG_tick_start_core1(0, 0, 0, 0);
//  return mrb_nil_value();
//}

static mrb_value
mrb_driver_stop(mrb_state *mrb, mrb_value self)
{
  PSG_tick_stop_core1();
  return mrb_nil_value();
}

/* Set LFO: tr, depth(cent), rate(0.1Hz) */
static mrb_value
mrb_driver_set_lfo(mrb_state *mrb, mrb_value self)
{
  mrb_int tr, depth, rate;
  mrb_int tick_delay = 0;
  mrb_get_args(mrb, "iii|i", &tr, &depth, &rate, &tick_delay);
  psg_packet_t p = {
    .tick = (uint32_t)tick_delay,
    .op   = PSG_PKT_LFO_SET,
    .reg  = (uint8_t)tr,
    .val  = (uint8_t)depth,
    .arg  = (uint8_t)rate,
  };
  return PSG_rb_push(&p) ? mrb_true_value() : mrb_false_value();
}

/* Set PAN */
static mrb_value
mrb_driver_set_pan(mrb_state *mrb, mrb_value self)
{
  mrb_int tr, pan;
  mrb_int tick_delay = 0;
  mrb_get_args(mrb, "ii|i", &tr, &pan, &tick_delay);
  if (tr < 0 || tr > 2 || pan < 0 || pan > 15) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "Invalid track or pan value: %d, %d", tr, pan);
  }
  psg_packet_t p = {
    .tick = (uint32_t)tick_delay,
    .op   = PSG_PKT_PAN_SET,
    .reg  = (uint8_t)tr,
    .val  = (uint8_t)pan,
  };
  return PSG_rb_push(&p) ? mrb_true_value() : mrb_false_value();
}

static mrb_value
mrb_driver_set_timbre(mrb_state *mrb, mrb_value self)
{
  mrb_int tr, timbre;
  mrb_int tick_delay = 0;
  mrb_get_args(mrb, "ii|i", &tr, &timbre, &tick_delay);
  if (tr < 0 || 2 < tr) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "Invalid track: %d (0-2 expected)", tr);
  }
  psg_packet_t p = {
    .tick = (uint32_t)tick_delay,
    .op   = PSG_PKT_TIMBRE_SET,
    .reg  = (uint8_t)tr,
    .val  = (uint8_t)timbre
  };
  return PSG_rb_push(&p) ? mrb_true_value() : mrb_false_value();
}

/* Mute on/off */
static mrb_value
mrb_driver_mute(mrb_state *mrb, mrb_value self)
{
  mrb_int tr, flag;
  mrb_int tick_delay = 0;
  mrb_get_args(mrb, "ii|i", &tr, &flag, &tick_delay);
  psg_packet_t p = {
    .tick = (uint32_t)tick_delay,
    .op   = PSG_PKT_CH_MUTE,
    .reg  = (uint8_t)tr,
    .val  = (uint8_t)flag,
  };
  return PSG_rb_push(&p) ? mrb_true_value() : mrb_false_value();
}

void
mrb_picoruby_psg_gem_init(mrb_state* mrb)
{
  struct RClass *module_PSG = mrb_define_module_id(mrb, MRB_SYM(PSG));
  struct RClass *class_Driver = mrb_define_class_under_id(mrb, module_PSG, MRB_SYM(Driver), mrb->object_class);

  mrb_define_const_id(mrb, class_Driver, MRB_SYM(CHIP_CLOCK), mrb_fixnum_value(CHIP_CLOCK));
  mrb_define_const_id(mrb, class_Driver, MRB_SYM(SAMPLE_RATE), mrb_fixnum_value(SAMPLE_RATE));

  mrb_value timbres = mrb_hash_new(mrb);
  mrb_hash_set(mrb, timbres, mrb_symbol_value(MRB_SYM(square)), mrb_fixnum_value(PSG_TIMBRE_SQUARE));
  mrb_hash_set(mrb, timbres, mrb_symbol_value(MRB_SYM(triangle)), mrb_fixnum_value(PSG_TIMBRE_TRIANGLE));
  mrb_hash_set(mrb, timbres, mrb_symbol_value(MRB_SYM(sawtooth)), mrb_fixnum_value(PSG_TIMBRE_SAWTOOTH));
  mrb_hash_set(mrb, timbres, mrb_symbol_value(MRB_SYM(invsawtooth)), mrb_fixnum_value(PSG_TIMBRE_INVSAWTOOTH));
  mrb_define_const_id(mrb, class_Driver, MRB_SYM(TIMBRES), timbres);

  mrb_define_class_method_id(mrb, class_Driver, MRB_SYM(select_pwm), mrb_driver_s_select_pwm, MRB_ARGS_REQ(2));
  mrb_define_class_method_id(mrb, class_Driver, MRB_SYM(select_mcp492x), mrb_driver_s_select_mcp492x, MRB_ARGS_REQ(5));
//  mrb_define_class_method_id(mrb, class_Driver, MRB_SYM(select_usbaudio), mrb_driver_s_select_usbaudio, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Driver, MRB_SYM(send_reg), mrb_driver_send_reg, MRB_ARGS_ARG(2, 1));
  mrb_define_method_id(mrb, class_Driver, MRB_SYM(stop), mrb_driver_stop, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Driver, MRB_SYM(set_lfo), mrb_driver_set_lfo, MRB_ARGS_ARG(3, 1));
  mrb_define_method_id(mrb, class_Driver, MRB_SYM(set_pan), mrb_driver_set_pan, MRB_ARGS_ARG(2, 1));
  mrb_define_method_id(mrb, class_Driver, MRB_SYM(set_timbre), mrb_driver_set_timbre, MRB_ARGS_ARG(2, 1));
  mrb_define_method_id(mrb, class_Driver, MRB_SYM(mute), mrb_driver_mute, MRB_ARGS_ARG(2, 1));
}

void
mrb_picoruby_psg_gem_final(mrb_state* mrb)
{
}

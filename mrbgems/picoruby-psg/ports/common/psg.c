/*
 * picoruby-psg/src/psg.c
 */

#include "../../include/psg.h"

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

psg_t psg;

psg_ringbuf_t rb = {
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

#define RESET_ENVELOPE(tr) \
  do { \
    psg.env_dir[tr]     = psg.env_attack[tr]; \
    psg.env_level[tr]   = psg.env_attack[tr] ? 0 : 15; \
    psg.env_running[tr] = true; \
    psg.env_cnt[tr]     = 0; \
  } while (0)

// AY compatible registors
static void
PSG_write_reg(uint8_t reg, uint8_t val)
{
  psg_cs_token_t tok = PSG_enter_critical();

  switch (reg) {
    /* ---- Tone period ---- */
    case 0:   /* tr A LSB */
      psg.r.tone_period[0] &= 0x0F00;
      psg.r.tone_period[0] |= val;
      break;
    case 1:   /* tr A MSB */
      psg.r.tone_period[0] &= 0x00FF;
      psg.r.tone_period[0] |= ((val & 0x0F) << 8);
      update_tone_inc(0);
      if ((psg.r.volume[0] & 0x10) && !psg.legato[0]) RESET_ENVELOPE(0);
      break;
    case 2:   /* tr B LSB */
      psg.r.tone_period[1] &= 0x0F00;
      psg.r.tone_period[1] |= val;
      break;
    case 3:   /* tr B MSB */
      psg.r.tone_period[1] &= 0x00FF;
      psg.r.tone_period[1] |= ((val & 0x0F) << 8);
      update_tone_inc(1);
      if ((psg.r.volume[1] & 0x10)  && !psg.legato[1]) RESET_ENVELOPE(1);
      break;
    case 4:   /* tr C LSB */
      psg.r.tone_period[2] &= 0x0F00;
      psg.r.tone_period[2] |= val;
      break;
    case 5:   /* tr C MSB */
      psg.r.tone_period[2] &= 0x00FF;
      psg.r.tone_period[2] |= ((val & 0x0F) << 8);
      update_tone_inc(2);
      if ((psg.r.volume[2] & 0x10) && !psg.legato[2]) RESET_ENVELOPE(2);
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
      uint8_t cont      = (val >> 3) & 1; // `continue` is a keyword
      uint8_t attack    = (val >> 2) & 1;
      uint8_t alternate = (val >> 1) & 1;
      uint8_t hold      =  val       & 1;
      for (int tr = 0; tr < 3; ++tr) {
        // Reset state machine on shape setting
        psg.env_continue[tr]  = cont;
        psg.env_attack[tr]    = attack;
        psg.env_alternate[tr] = alternate;
        psg.env_hold[tr]      = hold;
        RESET_ENVELOPE(tr);
      }
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
    case PSG_PKT_LEGATO_SET: {
      uint8_t tr = pkt->reg & 0x03;
      uint8_t legato = pkt->val; // 0=reset envelope, 1=no reset
      psg_cs_token_t t = PSG_enter_critical();
      psg.legato[tr] = (bool)legato;
      PSG_exit_critical(t);
      break;
    }
    default: // ignore?
      break;
  }
}

/* producer (core-0) ------------------------------------------------- */
bool
PSG_rb_push(const psg_packet_t *p)
{
  psg_cs_token_t t = PSG_enter_critical();
  uint16_t next = (rb.head + 1) & PSG_PACKET_QUEUE_MASK;
  if (next == rb.tail) {
    PSG_exit_critical(t);
    return false;  // full -> drop
  }
  rb.buf[rb.head] = *p;
  PSG_COMPILER_BARRIER();
  rb.head = next;
  PSG_exit_critical(t);
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
  psg_cs_token_t t = PSG_enter_critical();
  rb.tail = (rb.tail + 1) & PSG_PACKET_QUEUE_MASK;
  PSG_COMPILER_BARRIER();
  PSG_exit_critical(t);
}


// volume table: 1.5 dB steps with 3.0 dB headroom for 12-bit range
static const uint16_t vol_tab[16] = {
  0, 258, 307, 365, 434, 516, 613, 728, 865, 1029, 1223, 1453, 1727, 2052, 2439, 2899
};

// Pan table: 1 = L-only,  8 = center, 15 = R-only. 0 is not supposed to be used.
static const uint16_t pan_tab_l[16] = {
  4095, 4095, 4069, 3992, 3865, 3689, 3467, 3202, 2896, 2553, 2179, 1777, 1352,  911,  458,    0
};
static const uint16_t pan_tab_r[16] = {
     0,    0,  458,  911, 1352, 1777, 2179, 2553, 2896, 3202, 3467, 3689, 3865, 3992, 4069, 4095
};


// T = 1 / 22050 * 16 * EP
//   = 0.000723       * EP (s)
//   = 0.723          * EP (ms)
//   = 723            * EP (µs)
// FYI: T in MSX is `0.000143 * EP (s)` (256 / 1_789_770 * EP)
static inline void
update_envelope(void)
{
  for (int tr = 0; tr < 3; ++tr) {
    if ((psg.r.volume[tr] & 0x10) == 0) continue; // no envelope
    if (!psg.env_running[tr] || !psg.r.envelope_period) continue;
    if (++psg.env_cnt[tr] < psg.r.envelope_period) continue;

    psg.env_cnt[tr] = 0;

    /* 4µs - 1s (from datasheet): 1 period = 1 step (0‒15) */
    if (psg.env_dir[tr]) {                // up
      if (psg.env_level[tr] < 15) {
        ++psg.env_level[tr];
        return;
      }
    } else {                          // down
      if (psg.env_level[tr] > 0) {
        --psg.env_level[tr];
        return;
      }
    }

    // Reached the end of the envelope
    if (!psg.env_continue[tr]) {          // C = 0   -> Stop
      psg.env_running[tr] = false;
      return;
    }

    if (psg.env_hold[tr]) {               // C=1, H=1 -> No repeat
      psg.env_running[tr] = false;
      return;
    }

    if (psg.env_alternate[tr])            // Reverse direction
      psg.env_dir[tr] ^= 1;

    // Attack bit is valid only for the first cycle
    if (!psg.env_alternate[tr])
      psg.env_dir[tr] = psg.env_attack[tr]; // Sawtooth

    // In order to be faithful to the datasheet, advance one step after reversal
    if (psg.env_dir[tr]) {
      psg.env_level[tr] = 0;
    } else {
      psg.env_level[tr] = 15;
    }
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

// Soft clip by tanh approximation to avoid hard clipping
static inline uint32_t
soft_clip(uint32_t x)
{
  const uint32_t knee = 3600;
  if (x <= knee) return (uint16_t)x;
  uint32_t d = x - knee;
  // 0.117 ~= (1/8 − 1/128) -> (d>>3) − (d>>7)
  uint32_t y = knee + ((d >> 3) - (d >> 7));
  return (y > MAX_SAMPLE_WIDTH) ? MAX_SAMPLE_WIDTH : (uint16_t)y;
}

static inline void
PSG_calc_sample(uint16_t *l, uint16_t *r)
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
        // Extract upper 13 bits from 32 bit phase -> 0..8191
        uint32_t idx = psg.tone_phase[tr] >> 19; // = 32 - 13
        // XOR flipping by MSB: convert sawtooth to triangle
        idx ^= (idx >> 12);   // 0xxxxxxxxxxxx -> 0xxxxxxxxxxxx (as is)
                              // 1xxxxxxxxxxxx -> 0yyyyyyyyyyyy (flipped)
        // Extract lower 13 bits: 0..8191
        uint32_t tri = idx & 0x1FFF;
        // Normalize to 0..4095 (includes both even and odd)
        tone_amp = tri >> 1;
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
    if (vol & 0x10) vol = psg.env_level[tr];
    vol &= 0x0F;

    uint32_t gain = vol_tab[vol];
    uint32_t amp = (active_amp * gain) >> 12;

    // pan
    uint8_t bal = psg.pan[tr];          // 1..15
    mix_l += (amp * pan_tab_l[bal]) >> 12; // 0..4095
    mix_r += (amp * pan_tab_r[bal]) >> 12; // 0..4095
  }

  *l = (uint16_t)soft_clip(mix_l);
  *r = (uint16_t)soft_clip(mix_r);
}

void
PSG_render_block(uint32_t *dst, uint32_t samples)
{
  for (uint32_t i = 0; i < samples; i++) {
    uint16_t l, r;
    PSG_calc_sample(&l, &r);
    dst[i] = ((uint32_t)l << 16) | r;
  }
}

uint32_t pcm_buf[BUF_SAMPLES] = {0};
volatile uint32_t wr_idx = 0;
volatile uint32_t rd_idx = 0;

bool
PSG_audio_cb(void)
{
  if (psg_drv && psg_drv->write) {
    if (((rd_idx + 1) & BUF_MASK) != wr_idx) {
      uint32_t val = pcm_buf[rd_idx];
      rd_idx = (rd_idx + 1) & BUF_MASK;
      psg_drv->write(val>>16, val & 0xFFFF);
    }
  }
  return true;
}

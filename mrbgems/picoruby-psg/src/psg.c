/*
 * picoruby-psg/src/psg.c
 */

#include "../include/psg.h"
#include "../include/psg_ringbuf.h"

#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/class.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
  uint16_t tone_period[3];   // R0–5  (12-bit)
  uint8_t  noise_period;     // R6
  uint8_t  mixer;            // R7
  uint8_t  volume[3];        // R8–10
  uint16_t envelope_period;  // R11–12
  uint8_t  envelope_shape;   // R13
} psg_regs_t;

typedef struct {
  psg_regs_t r;
  uint32_t tone_inc[3];      // 32.32 fix
  uint32_t tone_phase[3];
  uint32_t noise_shift;
  uint32_t noise_cnt;
  uint32_t env_cnt;
  // envelope state machine
  uint8_t  env_level;     // 0‒15
  uint8_t  env_dir;       // 0=down, 1=up
  bool     env_running;   // false -> stop
  // Masked bit for performance
  uint8_t  env_continue;
  uint8_t  env_attack;
  uint8_t  env_alternate;
  uint8_t  env_hold;
} psg_t;

static psg_t psg;

static inline uint32_t
calc_inc(uint16_t period)
{
  if (!period) return 0;
  // f = clk / (16*(N)) -> inc = f / Fs (32.32)
  uint32_t f = CHIP_CLOCK / (16u * period);
  return ((uint64_t)f << 32) / SAMPLE_RATE;
}

static inline void
update_tone_inc(int ch)
{
  psg.tone_inc[ch] = calc_inc(psg.r.tone_period[ch]);
}

static psg_ringbuf_t rb;   /* BSS = 自動でゼロクリア */

/* AY 互換レジスタ書込み ------------------------------------------- */
void
PSG_write_reg(uint8_t reg, uint8_t val)
{
  psg_cs_token_t tok = PSG_enter_critical();   /* 排他開始 */

  switch (reg) {
  /* ---- Tone period ---- */
  case 0:   /* ch A LSB */
  case 1:   /* ch A MSB */
      psg.r.tone_period[0] &= reg ? 0x00FF : 0x0F00;
      psg.r.tone_period[0] |= reg ? ((val & 0x0F) << 8) : val;
      update_tone_inc(0);
      break;

  case 2:   /* ch B LSB */
  case 3:   /* ch B MSB */
      psg.r.tone_period[1] &= reg & 1 ? 0x00FF : 0x0F00;
      psg.r.tone_period[1] |= reg & 1 ? ((val & 0x0F) << 8) : val;
      update_tone_inc(1);
      break;

  case 4:   /* ch C LSB */
  case 5:   /* ch C MSB */
      psg.r.tone_period[2] &= reg & 1 ? 0x00FF : 0x0F00;
      psg.r.tone_period[2] |= reg & 1 ? ((val & 0x0F) << 8) : val;
      update_tone_inc(2);
      break;

  /* ---- Noise ---- */
  case 6:
      psg.r.noise_period = val & 0x1F;
      break;

  /* ---- Mixer ---- */
  case 7:
      psg.r.mixer = val;
      break;

  /* ---- Volume ---- */
  case 8:  case 9:  case 10:
      psg.r.volume[reg - 8] = val & 0x1F;
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
      /* 形状設定時にステートマシンをリセット */
      psg.env_continue  = (val >> 3) & 1;
      psg.env_attack    = (val >> 2) & 1;
      psg.env_alternate = (val >> 1) & 1;
      psg.env_hold      =  val       & 1;
      psg.env_dir       = psg.env_attack;
      psg.env_level     = psg.env_attack ? 0 : 15;
      psg.env_running   = true;
      psg.env_cnt       = 0;
      break;

  default:          /* R14–15 (I/O port) — 未実装 */
      break;
  }

  PSG_exit_critical(tok);             /* 排他終了 */
}

/* producer (core-0) ------------------------------------------------- */
bool
PSG_rb_push(const psg_packet_t *p)
{
  uint16_t next = (rb.head + 1) & PSG_PACKET_QUEUE_MASK;
  if (next == rb.tail) return false;                 /* full → drop */
  rb.buf[rb.head] = *p;
  PSG_COMPILER_BARRIER();
  rb.head = next;
  return true;
}

/* consumer (core-1) ------------------------------------------------- */
bool
PSG_rb_peak(psg_packet_t *out)   /* 参照だけ（pop しない） */
{
  if (rb.tail == rb.head) return false;              /* empty */
  *out = rb.buf[rb.tail];
  return true;
}

void
PSG_rb_pop(void)                   /* 実際に 1 slot 消費 */
{
  rb.tail = (rb.tail + 1) & PSG_PACKET_QUEUE_MASK;
  PSG_COMPILER_BARRIER();
}



// volume table: AY dB curve → 12-bit linear
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

static mrb_int pwm_pin;

static inline void
env_tick(void)
{
  if (!psg.env_running || !psg.r.envelope_period) return;

  if (++psg.env_cnt < psg.r.envelope_period) return;
  psg.env_cnt = 0;

  /* 4 µs〜1 s に相当（datasheet）: 1 period で 1 step (0‒15) */
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

  /* 端点に到達 */
  if (!psg.env_continue) {          // C = 0   → 完全停止
    psg.env_running = false;
    return;
  }

  if (psg.env_hold) {               // C=1, H=1 → 端点で保持し繰返し無し
    psg.env_running = false;
    return;
  }

  if (psg.env_alternate)            // 方向反転
    psg.env_dir ^= 1;

  /* Attack ビットは「1 周目の初期方向」だけに効く※ */
  if (!psg.env_alternate)
    psg.env_dir = psg.env_attack; // ノコギリ波

  /* 反転後に 1 step 進めておくと datasheet に忠実 */
  if (psg.env_dir) {
    psg.env_level = 0;
  } else {
    psg.env_level = 15;
  }
}

static void
set_envelope_shape(uint8_t shape)
{
  psg_cs_token_t t = PSG_enter_critical();

  psg.r.envelope_shape = shape & 0x0F;

  psg.env_continue  = (shape >> 3) & 1;
  psg.env_attack    = (shape >> 2) & 1;
  psg.env_alternate = (shape >> 1) & 1;
  psg.env_hold      =  shape       & 1;

  // initial direction and level
  psg.env_dir   = psg.env_attack;          // Attack=1 -> up
  psg.env_level = psg.env_attack ? 0 : 15; // 0->15  or 15->0
  psg.env_running = true;
  psg.env_cnt = 0;

  PSG_exit_critical(t);
}

bool
PSG_audio_cb(void)
{
  env_tick();

  // noise LFSR (17-bit)
  psg.noise_cnt++;
  if (psg.noise_cnt >= (psg.r.noise_period + 1)) {
    uint32_t fb = ((psg.noise_shift ^ (psg.noise_shift >> 3)) & 1);
    psg.noise_shift = (psg.noise_shift >> 1) | (fb << 16);
    psg.noise_cnt = 0;
  }
  uint8_t noise_bit = psg.noise_shift & 1;

  // mix
  uint32_t mix = 0;
  for (int ch = 0; ch < 3; ++ch) {
    // phase
    if (psg.tone_inc[ch]) {
      psg.tone_phase[ch] += psg.tone_inc[ch];
    }
    uint8_t tone_bit = (psg.tone_phase[ch] >> 31) & 1;

    bool use_tone  = !(psg.r.mixer & (1 << ch));
    bool use_noise = !(psg.r.mixer & (1 << (ch + 3)));

    uint8_t active = ((use_tone && tone_bit) || (use_noise && noise_bit));

    // volume: bit4=env
    uint8_t vol = psg.r.volume[ch];
    if (vol & 0x10) vol = psg.env_level;
    vol &= 0x0F;

    if (active) mix += vol_tab[vol];
  }
  if (mix > MAX_SAMPLE_WIDTH) mix = MAX_SAMPLE_WIDTH; // check overflow

  PSG_pwm_set_gpio_level(pwm_pin, mix);
  return true;
}

// external APIs

static void
set_tone_period(int ch, uint16_t period)
{
  psg_cs_token_t t = PSG_enter_critical();
  psg.r.tone_period[ch] = period;
  update_tone_inc(ch);
  PSG_exit_critical(t);
}

static void
set_noise_period(uint8_t period)
{
  psg_cs_token_t t = PSG_enter_critical();
  psg.r.noise_period = period;
  PSG_exit_critical(t);
}

static void
set_volume(int ch, uint8_t vol)
{
  psg_cs_token_t t = PSG_enter_critical();
  psg.r.volume[ch] = vol & 0x1F;   // 0-15 or 0x10|0-15
  PSG_exit_critical(t);
}


static mrb_value
mrb_psg_s_send_reg(mrb_state *mrb, mrb_value klass)
{
  mrb_int tick_delay, reg, val;
  mrb_get_args(mrb, "iii", &tick_delay, &reg, &val);
  psg_packet_t p = {
    .tick = (g_tick_ms + (uint8_t)tick_delay) & 0xFFFF,
    .op   = PSG_PKT_REG_WRITE,
    .reg  = (uint8_t)reg,
    .val  = (uint8_t)val,
  };
  if (PSG_rb_push(&p)) {
    return mrb_true_value();
  }
  return mrb_false_value();
}

static mrb_value
mrb_psg_s_launch_core1(mrb_state *mrb, mrb_value klass)
{
  mrb_get_args(mrb, "i", &pwm_pin);
  PSG_tick_start_core1((uint8_t)pwm_pin);
  return mrb_nil_value();
}

void
mrb_picoruby_psg_gem_init(mrb_state* mrb)
{
  struct RClass *class_PSG = mrb_define_module_id(mrb, MRB_SYM(PSG));

  mrb_define_class_method_id(mrb, class_PSG, MRB_SYM(send_reg), mrb_psg_s_send_reg, MRB_ARGS_REQ(3));
  mrb_define_class_method_id(mrb, class_PSG, MRB_SYM(launch_core1), mrb_psg_s_launch_core1, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_psg_gem_final(mrb_state* mrb)
{
}

/*
 * picoruby-psg/ports/rp2040/rp2040_rp2350_psg.c
 */

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"

#include <string.h>

#include "../../include/psg.h"
#include "../../include/psg_ringbuf.h"

// Critical section
#if defined(PICO_RP2040)

#include "pico/multicore.h"
#include "hardware/sync.h"

static spin_lock_t *psg_spin;

__attribute__((constructor))
static void
psg_port_init(void)
{
  psg_spin = spin_lock_instance(spin_lock_claim_unused(true));
}

psg_cs_token_t
PSG_enter_critical(void)
{
  uint32_t irq_state = save_and_disable_interrupts();
  spin_lock_unsafe_blocking(psg_spin);
  return irq_state; // token = IRQ
}

void
PSG_exit_critical(psg_cs_token_t token)
{
  spin_unlock(psg_spin);
  restore_interrupts(token);
}

#elif defined(PICO_RP2350)

#include "cmsis_gcc.h"
static volatile uint32_t psg_lock;

psg_cs_token_t
PSG_enter_critical(void)
{
  uint32_t irq_state = __get_PRIMASK();
  __disable_irq();
  // spin on lock
  while (__STREXW(1, &psg_lock)) { __CLREX(); }
  __DMB();
  return irq_state;
}

void
PSG_exit_critical(psg_cs_token_t token)
{
  __DMB();
  psg_lock = 0; // release
  if (!token) __enable_irq();
}

#endif


static repeating_timer_t audio_timer;

uint32_t
PSG_save_and_disable_interrupts(void)
{
  return save_and_disable_interrupts();
}

void
PSG_restore_interrupts(uint32_t irq_state)
{
  restore_interrupts(irq_state);
}

static bool
audio_cb(repeating_timer_t *t)
{
  (void)t;
  return PSG_audio_cb();
}

void
PSG_pwm_set_gpio_level(uint8_t gpio, uint16_t level)
{
  pwm_set_gpio_level((uint)gpio, level);
}

static void
audio_pwm_init(uint8_t gpio)
{
  gpio_set_function((uint)gpio, GPIO_FUNC_PWM);
  uint slice = pwm_gpio_to_slice_num((uint)gpio);
  pwm_set_wrap(slice, MAX_SAMPLE_WIDTH);
  pwm_set_enabled(slice, true);
}

void
PSG_add_repeating_timer(void)
{
  add_repeating_timer_us(-1000000 / SAMPLE_RATE, audio_cb, NULL, &audio_timer);
}

// Tick

volatile uint16_t g_tick_ms = 0;
static repeating_timer_t tick_timer;

static inline void
psg_process_packets(void)
{
  psg_packet_t pkt;
  while (PSG_rb_peak(&pkt)) {
    if (pkt.tick != g_tick_ms) break;   /* まだ先のイベント */
    PSG_rb_pop();
    if (pkt.op == PSG_PKT_REG_WRITE) {
      PSG_write_reg(pkt.reg, pkt.val);
    }
    /* 他 opcode は今回はスキップ */
  }
}

static bool
tick_cb(repeating_timer_t *t)
{
  (void)t;
  g_tick_ms++;
  psg_process_packets();      /* ← リングバッファ裁定 */
  return true;                /* keep repeating */
}

void PSG_tick_init_core1(void)
{
  /* Core1 専用 alarm_pool を確保（クロックは clk_ref=12MHz 固定） */
  static alarm_pool_t *pool = NULL;
  if (!pool) pool = alarm_pool_create(2 /* hardware timer 2 */, 16 /* IRQ prio */);
  /* 1 kHz = -1000 µs */
  alarm_pool_add_repeating_timer_us(pool, -1000, tick_cb, NULL, &tick_timer);
}

static volatile bool core1_alive = false;

static void __attribute__((noreturn))
psg_core1_main(void)
{
  /* 1) Core-0 から FIFO で PWM ピン番号を受け取る */
  uint8_t pin = (uint8_t) multicore_fifo_pop_blocking();
  /* 2) 出力初期化とタイマ開始 */
  audio_pwm_init(pin);       /* PWM + wrap */
  PSG_add_repeating_timer(); /* 22 kHz オーディオ割り込み */
  /* 3) 1 kHz tick (リングバッファ裁定) */
  PSG_tick_init_core1();
  /* 4) 何もしないループ ― 必要なら省電力 WFE などを挟む */
  for (;;)  tight_loop_contents();
}

void
PSG_tick_start_core1(uint8_t pin)
{
  if (!core1_alive) {
    /* 1) core-1 を起動（引数なし） */
    multicore_launch_core1(psg_core1_main);
    /* 2) FIFO で pin 番号を送る */
    multicore_fifo_push_blocking(pin);
    core1_alive = true;
  }
}

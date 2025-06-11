/*
 * picoruby-psg/ports/pico/psg_port.c
 */

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include "hardware/sync.h"

#include <string.h>
#include <stdlib.h>

#include "../../include/psg.h"

// Critical section
#if defined(PICO_RP2040)

#include "pico/multicore.h"
#include "hardware/sync.h"

static spin_lock_t *psg_spin;

/**
 * __attribute__((constructor)): Will be called before main() is executed.
 */
__attribute__((constructor))
static void
psg_port_init(void)
{
  /* Obtain a free HW spin-lock for inter-core critical sections:
   *   1) spin_lock_claim_unused(true)  -> find & reserve an unused lock ID (panic if none)
   *   2) spin_lock_instance(id)        -> convert that ID into a spin_lock_t* handle
   * Use the handle with spin_lock_blocking()/spin_unlock() to guard shared PSG state. */
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
static volatile uint32_t psg_lock = 0;

psg_cs_token_t
PSG_enter_critical(void)
{
  uint32_t irq_state = __get_PRIMASK();
  __disable_irq();
  return irq_state;
}

void
PSG_exit_critical(psg_cs_token_t token)
{
  if (!token) __enable_irq();
}

#endif


static repeating_timer_t audio_timer;

static bool
audio_cb(repeating_timer_t *t)
{
  (void)t;
  return PSG_audio_cb();
}

static void
psg_add_repeating_timer(void)
{
  add_repeating_timer_us(-1000000 / SAMPLE_RATE, audio_cb, NULL, &audio_timer);
}

// Tick

static volatile uint32_t g_tick_ms = 0;
static repeating_timer_t tick_timer;

static inline void
psg_process_packets(void)
{
  psg_packet_t pkt;
  while (PSG_rb_peek(&pkt)) {
    if (0 < (int32_t)(pkt.tick - g_tick_ms)) break;
    g_tick_ms = 0;
    PSG_rb_pop();
    PSG_process_packet(&pkt);
  }
}

static bool
tick_cb(repeating_timer_t *t)
{
  (void)t;
  g_tick_ms++;
  psg_process_packets();
  PSG_tick_1ms();   /* update internal LFO etc. */
//  if (psg_drv == &psg_drv_usbaudio) {
//    audio_task();
//  }
  return true;
}

static alarm_pool_t *tick_alarm_pool = NULL;

static void
psg_tick_init_core1(void)
{
  /* Core1 exclusive alarm_pool. clk_ref=12MHz */
  if (!tick_alarm_pool) {
    tick_alarm_pool = alarm_pool_create(2 /* hardware timer 2 */, 16 /* IRQ prio */);
    assert(tick_alarm_pool && "Failed to create alarm tick_alarm_pool");
  }
  memset(&tick_timer, 0, sizeof(tick_timer));
  /* 1 kHz = -1000 µs */
  if (!alarm_pool_add_repeating_timer_us(tick_alarm_pool, -1000, tick_cb, NULL, &tick_timer)) {
    assert(false && "Failed to add repeating timer");
  }
}

static volatile bool core1_alive = false;

#define ACK_CORE1_READY 0xA5

static void __attribute__((noreturn))
psg_core1_main(void)
{
  uint8_t p1 = (uint8_t)multicore_fifo_pop_blocking();
  uint8_t p2 = (uint8_t)multicore_fifo_pop_blocking();
  uint8_t p3 = (uint8_t)multicore_fifo_pop_blocking();
  uint8_t p4 = (uint8_t)multicore_fifo_pop_blocking();
  psg_drv->init(p1, p2, p3, p4); /* init PSG driver */
  psg_drv->start();
  psg_add_repeating_timer(); /* 22.05 kHz */
  psg_tick_init_core1();
  multicore_fifo_push_blocking(ACK_CORE1_READY);
  /* WFE? */
  for (;;) tight_loop_contents();
}

void
PSG_tick_start_core1(uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4)
{
  if (!core1_alive) {
    multicore_launch_core1(psg_core1_main);
    // send pin via FIFO
    multicore_fifo_push_blocking(p1);
    multicore_fifo_push_blocking(p2);
    multicore_fifo_push_blocking(p3);
    multicore_fifo_push_blocking(p4);
    uint32_t ack = multicore_fifo_pop_blocking();
    if (ack != ACK_CORE1_READY) {
      //printf("Unexpected core1 ready ack: 0x%08x\n", ack);
    }
    core1_alive = true;
  }
}

void
PSG_tick_stop_core1(void)
{
  if (psg_drv) {
    psg_drv->stop();
    psg_drv = NULL;
  }
  if (core1_alive) {
    // stop the repeating timer
    while (!cancel_repeating_timer(&tick_timer)) {
      tight_loop_contents();
    }
    memset(&tick_timer, 0, sizeof(tick_timer));
    if (tick_alarm_pool) {
      alarm_pool_destroy(tick_alarm_pool);
      tick_alarm_pool = NULL;
    }
    while (!cancel_repeating_timer(&audio_timer)) {
      tight_loop_contents();
    };
    memset(&audio_timer, 0, sizeof(audio_timer));
    multicore_reset_core1();
    core1_alive = false;
  }
}

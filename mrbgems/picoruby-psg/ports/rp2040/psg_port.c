/*
 * picoruby-psg/ports/pico/psg_port.c
 */

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include "hardware/sync.h"
#include "hardware/irq.h"

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

#define ALARM_AUDIO 1
#define ALARM_TICK  2

// Tick timer

static volatile uint32_t g_tick_ms = 0;
static repeating_timer_t tick_timer;

static inline void
psg_process_packets(void)
{
  psg_packet_t pkt;
  while (PSG_rb_peek(&pkt)) {
    if (0 < (int32_t)(pkt.tick - g_tick_ms)) break;
    g_tick_ms -= pkt.tick;
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

static alarm_pool_t *audio_alarm_pool = NULL;
static alarm_pool_t *tick_alarm_pool = NULL;

static volatile bool core1_alive = false;

enum {
  ACK_CORE1_READY = 0xA5,
  CMD_NONE,
  CMD_CLEANUP,
  ACK_DONE,
};

static void __attribute__((noreturn))
psg_core1_main(void)
{
  uint8_t p1 = (uint8_t)multicore_fifo_pop_blocking();
  uint8_t p2 = (uint8_t)multicore_fifo_pop_blocking();
  psg_drv->init(p1, p2); /* init PSG driver */
  psg_drv->start();

  if (!tick_alarm_pool) {
    tick_alarm_pool = alarm_pool_create(ALARM_TICK, 2);
    assert(tick_alarm_pool && "Failed to create alarm tick_alarm_pool");
    irq_set_priority(TIMER1_IRQ_2, 3);
  }
  memset(&tick_timer, 0, sizeof(repeating_timer_t));
  /* 1 kHz = -1000 µs */
  if (!alarm_pool_add_repeating_timer_us(tick_alarm_pool, -1000, tick_cb, NULL, &tick_timer)) {
    assert(false && "Failed to add repeating timer");
  }

  multicore_fifo_push_blocking(ACK_CORE1_READY);

  for (;;) {
    // core1 is responsible for rendering audio samples (heavy load)
    uint32_t used = (wr_idx - rd_idx) & BUF_MASK; // 0..255
    if (BUF_SAMPLES / 2 <= BUF_SAMPLES - used) {
      uint32_t dst_pos = wr_idx & BUF_MASK;  // Start position to write
      // How many words can we write at the end of the buffer?
      uint32_t first_len = MIN(BUF_SAMPLES - dst_pos, BUF_SAMPLES / 2);
      PSG_render_block(&pcm_buf[dst_pos], first_len);
      // If there are remaining words, write them at the beginning of the buffer
      if (first_len < BUF_SAMPLES / 2) {
        PSG_render_block(pcm_buf, (BUF_SAMPLES / 2 - first_len) / 2);
      }
      wr_idx += BUF_SAMPLES / 2;
    }

    tight_loop_contents();

    if (multicore_fifo_rvalid()) {
      uint32_t cmd = multicore_fifo_pop_blocking();
      if (cmd == CMD_CLEANUP) {
        cancel_repeating_timer(&tick_timer);
        alarm_pool_destroy(tick_alarm_pool);
        tick_alarm_pool  = NULL;
        multicore_fifo_push_blocking(ACK_DONE);
        for (;;) tight_loop_contents();
      }
    }
  }

}


// Audio timer

static repeating_timer_t audio_timer;

#if 1
#define DBG_PIN  5
#define DBG_TOGGLE()  (sio_hw->gpio_togl = 1u << DBG_PIN)
#else
#define DBG_TOGGLE()
#endif

static bool
audio_cb(repeating_timer_t *t)
{
  (void)t;
  DBG_TOGGLE();
  PSG_audio_cb();
  DBG_TOGGLE();
  return true;
}

void
PSG_tick_start_core1(uint8_t p1, uint8_t p2)
{
#if 1
  gpio_init(DBG_PIN);
  gpio_set_dir(DBG_PIN, GPIO_OUT);
  gpio_put(DBG_PIN, 0);
#endif

  PSG_render_block(pcm_buf, BUF_SAMPLES);
  wr_idx = BUF_SAMPLES;

  if (!core1_alive) {
    multicore_launch_core1(psg_core1_main);
    // send pin via FIFO
    multicore_fifo_push_blocking(p1);
    multicore_fifo_push_blocking(p2);
    uint32_t ack = multicore_fifo_pop_blocking();
    if (ack != ACK_CORE1_READY) {
      //printf("Unexpected core1 ready ack: 0x%08x\n", ack);
    }
    core1_alive = true;
  }

  if (!audio_alarm_pool) {
    audio_alarm_pool = alarm_pool_create(ALARM_AUDIO, 2);
    assert(audio_alarm_pool && "Failed to create alarm audio_alarm_pool");
    irq_set_priority(TIMER1_IRQ_1, 0);
  }
  memset(&audio_timer, 0, sizeof(repeating_timer_t));
  /* 22.05 kHz */
  if (!alarm_pool_add_repeating_timer_us(audio_alarm_pool, -1000000 / SAMPLE_RATE, audio_cb, NULL, &audio_timer)) {
    assert(false && "Failed to add repeating timer");
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
    multicore_fifo_push_blocking(CMD_CLEANUP);
    while (multicore_fifo_pop_blocking() != ACK_DONE) {
      // wait
    }
    multicore_reset_core1();

    cancel_repeating_timer(&audio_timer);
    alarm_pool_destroy(audio_alarm_pool);
    audio_alarm_pool = NULL;
    core1_alive = false;
  }
}

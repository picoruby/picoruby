/*
 * picoruby-psg/ports/pico/psg_port.c
 */

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include "hardware/sync.h"
#include "hardware/irq.h"
#include "pico/util/queue.h"

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

static alarm_pool_t *tick_alarm_pool = NULL;

static volatile bool core1_alive = false;

enum {
  ACK_CORE1_READY = 0xA5,
  CMD_NONE,
  CMD_CLEANUP,
  ACK_DONE,
};

queue_t fill_request_queue;

// Forward declaration for the driver
void dma_irq_feed_and_repoint(queue_t *q);

// This is the IRQ handler that will run on Core 1
static void __not_in_flash_func(core1_dma_irq_handler)(void) {
  dma_irq_feed_and_repoint(&fill_request_queue);
}

// Helper to format data before placing in DMA buffer
void format_and_fill_pcm_buf(int buffer_index) {
    uint32_t* target_buf = &pcm_buf[buffer_index * (BUF_SAMPLES / 2)];
    PSG_render_block(target_buf, BUF_SAMPLES / 2);
    for (int i = 0; i < BUF_SAMPLES / 2; i++) {
        uint16_t l = (target_buf[i] >> 16) & 0xFFFF;
        uint16_t r = target_buf[i] & 0xFFFF;
        uint16_t chA_cmd = 0x3000 | (l & 0x0FFF);
        uint16_t chB_cmd = 0xB000 | (r & 0x0FFF);
        target_buf[i] = ((uint32_t)chA_cmd << 16) | chB_cmd;
    }
}
static void __attribute__((noreturn))
psg_core1_main(void) {
  // 1. Pop pin config from Core 0
  uint8_t p1 = (uint8_t)multicore_fifo_pop_blocking();
  uint8_t p2 = (uint8_t)multicore_fifo_pop_blocking();

  // 2. Initialize communication queue
  queue_init(&fill_request_queue, sizeof(uint8_t), 2);

  // 3. Initialize the driver (PIO and DMA setup)
  psg_drv->init(p1, p2);

  // 4. Pre-fill both halves of the buffer with initial audio data
  format_and_fill_pcm_buf(0);
  format_and_fill_pcm_buf(1);
  
  // 5. Setup the 1ms tick timer for music logic
  tick_alarm_pool = alarm_pool_create(2, 1);
  alarm_pool_add_repeating_timer_us(tick_alarm_pool, -1000, tick_cb, NULL, &tick_timer);

  // 6. Signal Core 0 that we are ready
  multicore_fifo_push_blocking(0xA5A5);

  // 7. Start the driver, passing a pointer to OUR IRQ handler.
  // This is the crucial step that registers the IRQ on Core 1.
  psg_drv->start(core1_dma_irq_handler);

  // 8. Enter the main processing loop
  for (;;) {
    uint8_t buffer_index_to_fill;
    queue_remove_blocking(&fill_request_queue, &buffer_index_to_fill);
    format_and_fill_pcm_buf(buffer_index_to_fill);
  }
}


// Audio timer

void PSG_tick_start(uint8_t p1, uint8_t p2) {
  static bool core1_alive = false;
  if (!core1_alive) {
    multicore_launch_core1(psg_core1_main);
    multicore_fifo_push_blocking(p1);
    multicore_fifo_push_blocking(p2);
    (void)multicore_fifo_pop_blocking(); // Wait for Core 1 to be ready
    core1_alive = true;
  }
}

void
PSG_tick_stop(void)
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

    core1_alive = false;
  }
}

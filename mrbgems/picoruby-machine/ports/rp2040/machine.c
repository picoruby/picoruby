#include "../../include/hal.h"
#include "../../include/machine.h"
#include "../../include/ringbuffer.h"
#if defined(PICORB_VM_MRUBY)
#include "task.h"
#endif
#include "../../../picoruby-io-console/include/io-console.h"

#include "hardware/timer.h"
#include "pico/low_power.h"
#include "pico/stdlib.h"
#include "pico/mutex.h"
#include "hardware/irq.h"
#include "pico/unique_id.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "pico/aon_timer.h"
#if defined(PICO_RP2350)
#include "hardware/powman.h"
#endif
#include <sys/time.h>
#if defined(PICO_CYW43_ARCH_POLL)
#include "pico/cyw43_arch.h"
#endif

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <tusb.h>

#include "hardware/gpio.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"

/*-------------------------------------
 *
 * Signal flag (shared with machine.h)
 *
 *------------------------------------*/

volatile int sigint_status = 0; /* MACHINE_SIG_NONE */

/*-------------------------------------
 *
 * stdin RingBuffer (ISR-driven input)
 *
 *------------------------------------*/

/* Must be a power of two AND >= CFG_TUD_CDC_RX_BUFSIZE (512) so that
 * tud_cdc_rx_cb() can drain the entire CDC FIFO without overflow. */
#ifndef PICORB_STDIN_BUFFER_SIZE
#define PICORB_STDIN_BUFFER_SIZE 1024
#endif

static uint8_t stdin_buf_mem[sizeof(RingBuffer) + PICORB_STDIN_BUFFER_SIZE]
  __attribute__((aligned(4)));
static RingBuffer *stdin_rb = (RingBuffer *)stdin_buf_mem;

bool
picorb_hal_stdin_push(uint8_t ch)
{
  /* Only intercept signal chars in cooked mode (like POSIX).
   * In raw mode, pass all bytes through for binary data (e.g. DFU). */
  if (!io_raw_q()) {
    if (ch == 3) {
      sigint_status = MACHINE_SIGINT_RECEIVED;
      return true;  /* signal consumed, not an overflow */
    }
    if (ch == 26) {
      sigint_status = MACHINE_SIGTSTP_RECEIVED;
      return true;
    }
  }
  return RingBuffer_push(stdin_rb, ch);
}

/*-------------------------------------
 *
 * Canonical (cooked mode) line buffer
 *
 *------------------------------------*/

#ifndef PICORB_CANONICAL_BUF_SIZE
#define PICORB_CANONICAL_BUF_SIZE 256
#endif

static uint8_t canon_buf[PICORB_CANONICAL_BUF_SIZE];
static int canon_len = 0;
static int canon_read_pos = 0;
static bool canon_eof = false;

#define CANON_LINE_READY  1
#define CANON_EOF         2
#define CANON_ACCUMULATING 0

/*
 * Process one raw byte in canonical (cooked) mode.
 * Handles echo, backspace editing, Ctrl-D (EOF).
 * Returns CANON_LINE_READY, CANON_EOF, or CANON_ACCUMULATING.
 */
static int
canon_process_char(uint8_t raw)
{
  if (raw == 8 || raw == 127) {
    /* Backspace / DEL */
    if (canon_len > 0) {
      canon_len--;
      if (io_echo_q()) {
        picorb_hal_write(1, "\b \b", 3);
      }
    }
    return CANON_ACCUMULATING;
  }
  if (raw == '\n' || raw == '\r') {
    if (canon_len < PICORB_CANONICAL_BUF_SIZE) {
      canon_buf[canon_len++] = raw;
    }
    if (io_echo_q()) {
      picorb_hal_write(1, "\r\n", 2);
    }
    canon_read_pos = 0;
    return CANON_LINE_READY;
  }
  if (raw == 4) {
    /* Ctrl-D */
    if (canon_len == 0) {
      canon_eof = true;
      return CANON_EOF;
    }
    /* Flush partial line without newline */
    canon_read_pos = 0;
    return CANON_LINE_READY;
  }
  if (raw == 27) {
    /* ESC: ignore */
    return CANON_ACCUMULATING;
  }
  /* Printable or other control chars: append */
  if (canon_len < PICORB_CANONICAL_BUF_SIZE) {
    canon_buf[canon_len++] = raw;
    if (io_echo_q()) {
      picorb_hal_write(1, &raw, 1);
    }
  }
  return CANON_ACCUMULATING;
}

/*-------------------------------------
 *
 * HAL
 *
 *------------------------------------*/

#ifdef MRBC_NO_TIMER
  #error "MRBC_NO_TIMER is not supported"
#endif

#define ALARM_NUM 0
#define ALARM_IRQ timer_hardware_alarm_get_irq_num(timer_hw, ALARM_NUM)

#ifndef MRBC_TICK_UNIT
#define MRBC_TICK_UNIT 1
#endif
#define US_PER_MS (MRBC_TICK_UNIT * 1000)

/*-------------------------------------
 *
 * USB pump - single-owner tud_task()
 *
 * TinyUSB's dcd IRQ handler (highest-order shared handler on
 * USBCTRL_IRQ, installed by tud_init) services the hardware and
 * enqueues events at interrupt level; tud_task() drains that queue and
 * runs the class-driver callbacks. Historically PicoRuby also ran
 * tud_task() directly inside the USB IRQ *and* from several
 * thread-level paths (getchar, signal polling, Machine.tud_task), so
 * the TinyUSB device stack (OPT_OS_NONE, no internal locking) was
 * re-entered across contexts.
 *
 * Now tud_task() has a single owner: usb_pump_worker(), running in a
 * claimed spare user IRQ at the lowest priority. Everything else - the
 * USB IRQ, the 1 ms tick, and thread-level code - only *requests* a
 * pump via irq_set_pending(). usb_mutex serializes the pump against
 * the thread-level CDC write path; the worker uses a non-blocking
 * try-lock and simply retires when a thread holds the mutex (that
 * thread pumps by itself, and the next tick re-pends the worker, so a
 * pump is never lost). This mirrors pico-sdk's pico_stdio_usb design.
 *------------------------------------*/

static mutex_t usb_mutex;
static int usb_pump_irq_num = -1;
/* True while the current context holds usb_mutex and is inside
 * tud_task()/the FIFO drain: callbacks that run from there (e.g. the
 * input echo in picorb_hal_stdin_push -> picorb_hal_write) must not
 * take the mutex again. Single core, so a plain flag is exact. */
static volatile bool in_usb_pump = false;

static void
usb_pump_request(void)
{
  if (usb_pump_irq_num >= 0) {
    irq_set_pending((uint)usb_pump_irq_num);
  }
}

/* Move CDC RX bytes into the stdin ring buffer. Runs only under
 * usb_mutex. tud_cdc_rx_cb() pushes on arrival; this catches bytes it
 * had to leave in the CDC FIFO while the ring buffer was full. */
static void
usb_cdc_drain(void)
{
  while (tud_cdc_available() && RingBuffer_free_size(stdin_rb) > 1) {
    uint8_t cdc_ch = (uint8_t)tud_cdc_read_char();
    picorb_hal_stdin_push(cdc_ch);
  }
}

static void
usb_pump_worker(void)
{
  if (mutex_try_enter(&usb_mutex, NULL)) {
    in_usb_pump = true;
    tud_task();
    usb_cdc_drain();
    in_usb_pump = false;
    mutex_exit(&usb_mutex);
  }
  /* else: a thread-level holder is pumping; the 1 ms tick re-pends. */
}

static volatile uint32_t interrupt_nesting = 0;

static volatile bool in_tick_processing = false;

#if defined(PICORB_VM_MRUBY)
static mrb_state *mrb_;
#endif

static void
alarm_handler(void)
{
  /* 1 ms backstop for the USB pump: guarantees TinyUSB servicing even
   * when the VM computes without touching IO, and re-pends a pump that
   * backed off while a thread held usb_mutex. One NVIC write. */
  usb_pump_request();

  if (in_tick_processing) {
    timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + US_PER_MS;
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);
    return;
  }

  in_tick_processing = true;
  __dmb();

  uint32_t current_time = timer_hw->timerawl;
  uint32_t next_time = current_time + US_PER_MS;
  timer_hw->alarm[ALARM_NUM] = next_time;
  hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);

#if defined(PICORB_VM_MRUBY)
  picorb_tick(mrb_);
#elif defined(PICORB_VM_MRUBYC)
  picorb_tick();
#else
#error "One of PICORB_VM_MRUBY or PICORB_VM_MRUBYC must be defined"
#endif

  __dmb();
  in_tick_processing = false;
}

static void
usb_irq_handler(void)
{
  /* Registered LOWEST_ORDER on USBCTRL_IRQ: TinyUSB's dcd handler
   * (highest order) has already serviced the hardware and enqueued
   * events; all we do is schedule the pump to drain them. */
  usb_pump_request();
}

#if defined(PICORB_VM_MRUBY) && defined(PICO_CYW43_ARCH_POLL)
/* cyw43_arch POLL mode: cyw43_driver, lwIP and btstack share one
 * async_context that is only serviced from thread context. Pump it at
 * every scheduler entry so a compute-bound task or a long GC drain
 * cannot starve the network/BLE stack. Guarded by
 * cyw43_is_initialized() so this is a no-op until Wi-Fi/BLE has been
 * brought up. Must stay cheap and never sleep -- idling belongs to
 * picorb_hal_idle_cpu(). */
static void
rp2_scheduler_service(mrb_state *mrb, void *ud)
{
  (void)mrb;
  (void)ud;
  if (cyw43_is_initialized(&cyw43_state)) {
    cyw43_arch_poll();
  }
}
#endif

void
#if defined(PICORB_VM_MRUBY)
picorb_hal_init(mrb_state *mrb)
#elif defined(PICORB_VM_MRUBYC)
picorb_hal_init(void)
#endif
{
#if defined(PICORB_VM_MRUBY)
  mrb_ = (mrb_state *)mrb;
#if defined(PICO_CYW43_ARCH_POLL)
  mrb_task_set_scheduler_hook(mrb, rp2_scheduler_service, NULL);
#endif
#endif
  RingBuffer_init(stdin_rb, PICORB_STDIN_BUFFER_SIZE);
  /* Own alarm 0 in the SDK's claim registry, permanently. Disabling
   * the NVIC IRQ is not ownership: pico_low_power claims an unused
   * alarm internally and would otherwise be free to pick this one
   * and overwrite the tick. */
  hardware_alarm_claim(ALARM_NUM);
  hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);
  irq_set_exclusive_handler(ALARM_IRQ, alarm_handler);
  irq_set_enabled(ALARM_IRQ, true);
  irq_set_priority(ALARM_IRQ, PICO_HIGHEST_IRQ_PRIORITY);
  timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + US_PER_MS;

  clocks_hw->sleep_en0 = 0;
#if defined(PICO_RP2040)
  clocks_hw->sleep_en1 =
  CLOCKS_SLEEP_EN1_CLK_SYS_TIMER_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_USBCTRL_BITS
  | CLOCKS_SLEEP_EN1_CLK_USB_USBCTRL_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_UART0_BITS
  | CLOCKS_SLEEP_EN1_CLK_PERI_UART0_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_UART1_BITS
  | CLOCKS_SLEEP_EN1_CLK_PERI_UART1_BITS;
#elif defined(PICO_RP2350)
  clocks_hw->sleep_en1 =
  CLOCKS_SLEEP_EN1_CLK_SYS_TIMER0_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_TIMER1_BITS
  /* The TICKS block feeds the system timers their 1MHz tick. Without
   * these two bits it stops whenever a core sleeps, the timers stop
   * counting, no alarm ever fires, and WFI/WFE never wake -- the
   * long-standing "__wfi does not wake up on RP2350" mystery. RP2040
   * has no TICKS block (its timer tick comes from the watchdog). */
  | CLOCKS_SLEEP_EN1_CLK_REF_TICKS_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_TICKS_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_USBCTRL_BITS
  | CLOCKS_SLEEP_EN1_CLK_USB_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_UART0_BITS
  | CLOCKS_SLEEP_EN1_CLK_PERI_UART0_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_UART1_BITS
  | CLOCKS_SLEEP_EN1_CLK_PERI_UART1_BITS;
#else
  #error "Unsupported Board"
#endif

  mutex_init(&usb_mutex);
  usb_pump_irq_num = (int)user_irq_claim_unused(true);
  irq_set_exclusive_handler((uint)usb_pump_irq_num, usb_pump_worker);
  irq_set_priority((uint)usb_pump_irq_num, PICO_LOWEST_IRQ_PRIORITY);
  irq_set_enabled((uint)usb_pump_irq_num, true);

  tud_init(TUD_OPT_RHPORT);
  irq_add_shared_handler(USBCTRL_IRQ, usb_irq_handler,
      PICO_SHARED_IRQ_HANDLER_LOWEST_ORDER_PRIORITY);
}

#if defined(PICORB_VM_MRUBY)
void
picorb_hal_final(mrb_state *mrb)
{
  (void)mrb;
}
#endif

void
picorb_hal_enable_irq(void)
{
  if (interrupt_nesting == 0) {
//    return; // wrong state???
  }
  interrupt_nesting--;
  if (interrupt_nesting > 0) {
    return;
  }
  __dmb();
  asm volatile ("cpsie i" : : : "memory");
}

void
picorb_hal_disable_irq(void)
{
  asm volatile ("cpsid i" : : : "memory");
  __dmb();
  interrupt_nesting++;
}

void
#if defined(PICORB_VM_MRUBYC)
picorb_hal_idle_cpu()
#elif defined(PICORB_VM_MRUBY)
picorb_hal_idle_cpu(mrb_state *mrb)
#endif
{
#if defined(PICO_CYW43_ARCH_POLL)
  /* cyw43_arch POLL mode: pump the shared async_context (cyw43_driver, lwIP and
     btstack) then block until there is new stack work or the next scheduler
     tick, whichever comes first. Unlike bare __wfi this self-bounds on the
     deadline via an armed timer, so it also sidesteps the RP2350
     __wfi-does-not-wake issue below. No-op until the stack is brought up. */
  if (cyw43_is_initialized(&cyw43_state)) {
    cyw43_arch_poll();
    cyw43_arch_wait_for_work_until(make_timeout_time_ms(MRBC_TICK_UNIT));
    return;
  }
#endif
  /* Plain WFI on both chips. This historically hung on RP2350 -- not
   * because of WFI itself, but because the sleep_en1 mask set in
   * picorb_hal_init gated the TICKS block while the core slept: the
   * timers stopped counting, so the 1ms alarm could never fire and
   * nothing woke the core. With CLK_*_TICKS kept enabled during
   * sleep, the tick IRQ wakes WFI normally. */
  __wfi();
}

#if defined(PICORB_VM_MRUBY)
void
picorb_hal_sleep_us(mrb_state *mrb, mrb_int usec)
{
  (void)mrb;
  sleep_us((uint32_t)usec);
}
#endif

int
picorb_hal_cdc_write(uint8_t itf, const void *buf, int nbytes, uint32_t timeout_ms)
{
  if (nbytes <= 0) {
    return 0;
  }
  if (in_usb_pump) {
    /* Called from inside the pump (e.g. the input echo that
     * tud_cdc_rx_cb triggers via picorb_hal_stdin_push): the mutex is
     * already held by this context. Write what fits; dropping under
     * flood matches the previous behavior of this path. */
    tud_cdc_n_write(itf, buf, (uint32_t)nbytes);
    return (int)tud_cdc_n_write_flush(itf);
  }
  bool locked = (timeout_ms == 0) ? mutex_try_enter(&usb_mutex, NULL) : mutex_enter_timeout_ms(&usb_mutex, 100);
  if (!locked) {
    return 0;
  }
  int written = 0;
  const uint8_t *p = (const uint8_t *)buf;
  absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
  while (written < nbytes) {
    uint32_t avail = tud_cdc_n_write_available(itf);
    if (avail > 0) {
      uint32_t n = (uint32_t)(nbytes - written);
      if (n > avail) n = avail;
      uint32_t n2 = tud_cdc_n_write(itf, p + written, n);
      tud_cdc_n_write_flush(itf);
      written += (int)n2;
    }
    else {
      /* TX FIFO full. If no host is listening, or it stopped reading
       * for too long, give up instead of stalling the VM. Otherwise
       * pump TinyUSB ourselves (we hold the mutex) so the endpoint
       * transfer completes and frees FIFO space. */
      if (!tud_cdc_n_connected(itf) || time_reached(deadline)) {
        break;
      }
      in_usb_pump = true;
      tud_task();
      in_usb_pump = false;
      tud_cdc_n_write_flush(itf);
    }
  }
  mutex_exit(&usb_mutex);
  return written;
}

bool
picorb_hal_cdc_connected(uint8_t itf)
{
  return tud_cdc_n_connected(itf);
}

int
picorb_hal_write(int fd, const void *buf, int nbytes)
{
#if CFG_TUD_CDC >= 2
  // Use separate CDC instances: stdout -> CDC0, stderr -> CDC1
  uint8_t itf = (fd == FD_STDERR) ? CDC_INSTANCE_STDERR : CDC_INSTANCE_STDOUT;
#else
  const uint8_t itf = 0;
#endif
  return picorb_hal_cdc_write(itf, buf, nbytes, 500);
}

int picorb_hal_flush(int fd) {
#if CFG_TUD_CDC >= 2
  uint8_t itf = (fd == FD_STDERR) ? CDC_INSTANCE_STDERR : CDC_INSTANCE_STDOUT;
#else
  const uint8_t itf = 0;
#endif
  if (in_usb_pump) {
    return (int)tud_cdc_n_write_flush(itf);
  }
  if (!mutex_enter_timeout_ms(&usb_mutex, 100)) {
    return 0;
  }
  int len = (int)tud_cdc_n_write_flush(itf);
  mutex_exit(&usb_mutex);
  return len;
}

int
picorb_hal_read_available(void)
{
  if (io_raw_q()) {
    return (RingBuffer_data_size(stdin_rb) > 0) ? 1 : 0;
  }
  /* Cooked mode: data available if canon_buf has unread bytes */
  if (canon_read_pos < canon_len) {
    return 1;
  }
  /* Check if ringbuffer contains a line terminator */
  if (RingBuffer_search_char(stdin_rb, '\n') >= 0 ||
      RingBuffer_search_char(stdin_rb, '\r') >= 0 ||
      RingBuffer_search_char(stdin_rb, 4) >= 0) {
    return 1;
  }
  return 0;
}

int
picorb_hal_getchar(void)
{
  /* Input arrives via the USB pump (tud_cdc_rx_cb / usb_cdc_drain fill
   * the stdin ring buffer); this function only consumes the buffer, so
   * the CDC FIFO has exactly one consumer. Ask for a pump so leftover
   * FIFO bytes (ring buffer was full) get another chance now that the
   * caller is consuming. */
  usb_pump_request();

  if (sigint_status == MACHINE_SIGINT_RECEIVED) {
    sigint_status = MACHINE_SIG_NONE;
    return 3;
  }
  if (sigint_status == MACHINE_SIGTSTP_RECEIVED) {
    sigint_status = MACHINE_SIG_NONE;
    return 26;
  }

  if (io_raw_q()) {
    /* Raw mode: pass through directly */
    uint8_t ch;
    if (RingBuffer_pop(stdin_rb, &ch)) {
      return (int)ch;
    }
    return HAL_GETCHAR_NODATA;
  }

  /* Cooked mode */
  /* Serve buffered characters first */
  if (canon_read_pos < canon_len) {
    uint8_t ch = canon_buf[canon_read_pos++];
    if (canon_read_pos >= canon_len) {
      /* Line fully consumed, reset buffer */
      canon_len = 0;
      canon_read_pos = 0;
    }
    return (int)ch;
  }

  /* Check for pending EOF */
  if (canon_eof) {
    canon_eof = false;
    return HAL_GETCHAR_EOF;
  }

  /* Try to read one byte from ringbuffer and process */
  uint8_t raw;
  if (!RingBuffer_pop(stdin_rb, &raw)) {
    return HAL_GETCHAR_NODATA;
  }

  int result = canon_process_char(raw);
  switch (result) {
    case CANON_LINE_READY:
      /* Start serving from canon_buf */
      return picorb_hal_getchar();
    case CANON_EOF:
      canon_eof = false;
      return HAL_GETCHAR_EOF;
    default:
      /* Still accumulating line */
      return HAL_GETCHAR_NODATA;
  }
}

void
picorb_hal_abort(const char *s)
{
  if( s ) {
    picorb_hal_write(1, s, strlen(s));
  }

  abort();
}


/*-------------------------------------
 *
 * USB
 *
 *------------------------------------*/

void
Machine_tud_task(void)
{
  /* Public "kick the USB pump" entry (Machine.tud_task from Ruby, BLE
   * init, io_wait_for_input, signal polling). Pumps synchronously when
   * the mutex is free so existing callers keep their semantics;
   * otherwise defers to the pump IRQ. */
  if (in_usb_pump) {
    return;
  }
  if (mutex_enter_timeout_ms(&usb_mutex, 10)) {
    in_usb_pump = true;
    tud_task();
    usb_cdc_drain();
    in_usb_pump = false;
    mutex_exit(&usb_mutex);
  }
  else {
    usb_pump_request();
  }
}

bool
Machine_tud_mounted_q(void)
{
  return tud_mounted();
}

/*
 * Read the BOOTSEL button via the QSPI chip select pin.
 * Based on tinyusb BSP (hw/bsp/rp2040/family.c).
 * Weak symbol: if tinyusb_board provides get_bootsel_button,
 * the linker uses that version instead.
 *
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 */
__attribute__((weak))
bool __no_inline_not_in_flash_func(get_bootsel_button)(void) {
  const uint CS_PIN_INDEX = 1;

  uint32_t flags = save_and_disable_interrupts();

  hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                  GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                  IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

  for (volatile int i = 0; i < 1000; ++i);

#ifdef __ARM_ARCH_6M__
  #define CS_BIT (1u << 1)
#else
  #define CS_BIT SIO_GPIO_HI_IN_QSPI_CSN_BITS
#endif
  bool button_state = (sio_hw->gpio_hi_in & CS_BIT);

  hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                  GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                  IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

  restore_interrupts(flags);

  return button_state;
}

bool
Machine_bootsel_pressed_q(void)
{
  return !get_bootsel_button();
}


/*-------------------------------------
 *
 * Sleep in low power mode (pico_low_power, SDK >= 2.3.0)
 *
 *------------------------------------*/

/* GPIO suspend/resume hooks. The strong definitions live in
 * picoruby-irq's rp2040 port; a build without that gem gets these
 * empty defaults. The SDK's GPIO wake path steals the per-core GPIO
 * callback and disables the wake pin's events afterwards, so the irq
 * gem must take its registrations offline before the SDK call and
 * rebuild them after. */
__attribute__((weak)) void picorb_irq_gpio_suspend(void) {}
__attribute__((weak)) void picorb_irq_gpio_resume(void) {}

static machine_sleep_result_t
sleep_result_from_sdk(int rc)
{
  if (rc == 0) return MACHINE_SLEEP_OK;
  if (rc == PICO_ERROR_INSUFFICIENT_RESOURCES) return MACHINE_SLEEP_ERESOURCE;
  /* PRECONDITION_NOT_MET, INVALID_DATA, and the near-minimum
   * INVALID_ARG race are machine-state problems by the time we get
   * here: the caller's arguments were already validated. */
  return MACHINE_SLEEP_ESTATE;
}

/* timespec -> signed 64-bit microseconds, failing closed. */
static bool
timespec_to_us64(const struct timespec *ts, int64_t *us)
{
  if (ts->tv_sec < 0) return false;
  int64_t v = (int64_t)ts->tv_sec;
  if (INT64_MAX / 1000000 < v) return false;
  *us = v * 1000000 + ts->tv_nsec / 1000;
  return true;
}

/* Sync the AON timer to libc realtime, remembering both sides.
 * "Running" does not mean "correct": ordinary WFI idle and timer
 * SLEEP both leave the AON clock unclocked, so it lags realtime.
 * Called before each DORMANT cell -- the only AON consumers. */
static bool
aon_presync(int64_t *libc_before_us, int64_t *aon_before_us)
{
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0) return false;
  struct timespec ts;
  ts.tv_sec = tv.tv_sec;
  ts.tv_nsec = (long)tv.tv_usec * 1000;
  if (aon_timer_is_running()) {
    if (!aon_timer_set_time(&ts)) return false;
  } else {
    if (!aon_timer_start(&ts)) return false;
  }
  /* Read BACK what the AON actually stored: the RP2040 RTC keeps
   * whole seconds only, and the delta must be computed against the
   * stored value, not the requested one. */
  struct timespec aon_ts;
  if (!aon_timer_get_time(&aon_ts)) return false;
  int64_t libc_us, aon_us;
  if (!timespec_to_us64(&ts, &libc_us)) return false;
  if (!timespec_to_us64(&aon_ts, &aon_us)) return false;
  *libc_before_us = libc_us;
  *aon_before_us = aon_us;
  return true;
}

/* Apply libc_before + (aon_after - aon_before) to libc realtime.
 * Delta-based, not absolute: setting libc to the AON's absolute value
 * would drop sub-seconds on RP2040 (1s resolution) and could step
 * time backwards after a short sleep. */
static bool
aon_postsync(int64_t libc_before_us, int64_t aon_before_us)
{
  struct timespec aon_ts;
  if (!aon_timer_get_time(&aon_ts)) return false;
  int64_t aon_after_us;
  if (!timespec_to_us64(&aon_ts, &aon_after_us)) return false;
  if (aon_after_us < aon_before_us) return false;
  int64_t delta = aon_after_us - aon_before_us;
  if (INT64_MAX - delta < libc_before_us) return false;
  int64_t now_us = libc_before_us + delta;
  struct timeval tv;
  tv.tv_sec = (time_t)(now_us / 1000000);
  tv.tv_usec = (suseconds_t)(now_us % 1000000);
  return settimeofday(&tv, NULL) == 0;
}

/* One worker for all four cells of the mode x wake-source matrix.
 * Straight-line with acquisition flags; the single cleanup at the
 * bottom unwinds whatever was taken, in reverse order, on success
 * and error alike. */
static machine_sleep_result_t
machine_sleep_run(bool deep, bool use_timer, uint32_t ms, int pin, bool edge, bool high)
{
  machine_sleep_result_t result;
  bool alarm_stopped = false;
  bool mask_saved = false;
  bool usb_locked = false;
  bool gpio_suspended = false;
#if defined(PICO_RP2350)
  bool lposc_switched = false;
#endif
  bool aon_synced = false;
  uint32_t saved_en0 = 0, saved_en1 = 0;
  int64_t libc_before_us = 0, aon_before_us = 0;

  /* Early validation: nothing acquired yet, plain returns. */
  if (!use_timer) {
    if (pin < 0 || NUM_BANK0_GPIOS <= pin) return MACHINE_SLEEP_EINVAL;
  }
  if (use_timer && deep && ms < PICO_LOW_POWER_MIN_DORMANT_TIME_MS) {
    return MACHINE_SLEEP_EINVAL;
  }

  /* Both DORMANT cells stop the system timer, so the wall clock must
   * survive via the AON timer: sync it to realtime now, delta-correct
   * realtime after wake. The SLEEP cells keep the system timer
   * clocked and need none of this. */
  if (deep) {
    if (!aon_presync(&libc_before_us, &aon_before_us)) {
      return MACHINE_SLEEP_ESTATE;
    }
    aon_synced = true;
  }

  /* The 1 ms tick must not run while the SDK owns clocks and IRQs. */
  irq_set_enabled(ALARM_IRQ, false);
  alarm_stopped = true;

  /* The SDK's wake path sets sleep_en0/1 to all-ones; the HAL's
   * boot-time mask is ours to put back. */
  saved_en0 = clocks_hw->sleep_en0;
  saved_en1 = clocks_hw->sleep_en1;
  mask_saved = true;

  if (deep) {
    /* Dormant tears TinyUSB down (tud_deinit/tud_init) from thread
     * context BEFORE it disables interrupts; holding usb_mutex keeps
     * the pump worker and the CDC write path out of that window. */
    mutex_enter_blocking(&usb_mutex);
    usb_locked = true;
  }

  if (!use_timer) {
    /* Take the irq gem's GPIO registrations offline: the SDK owns
     * the shared IO_BANK0 IRQ and the global callback for the
     * duration. Consequence, documented in the README: EDGE events
     * on other pins during the sleep are discarded; LEVEL conditions
     * still asserted at resume fire normally. */
    picorb_irq_gpio_suspend();
    gpio_suspended = true;
  }

  int rc;
  if (use_timer) {
    if (deep) {
      /* RTC source on RP2040 (XOSC kept only for clk_rtc), LPOSC on
       * RP2350; the SDK handles the powman tick source itself on
       * this path. */
      rc = low_power_dormant_for_ms(ms, DORMANT_CLOCK_SOURCE_DEFAULT, NULL);
    } else {
      /* exclusive=false: a foreign IRQ (USB) is serviced and the SDK
       * returns to WFI until its own alarm fires, so the console
       * stays alive through the sleep. */
      rc = low_power_sleep_for_ms(ms, NULL, false);
    }
  } else {
    clock_dest_bitset_t keep = clock_dest_bitset_none();
    if (deep) {
      /* The SDK adds the AON clock destination only on its timer
       * paths; without this the wall clock stops. */
#if defined(PICO_RP2040)
      clock_dest_bitset_add(&keep, CLK_DEST_RTC_RTC);
#elif defined(PICO_RP2350)
      clock_dest_bitset_add(&keep, CLK_DEST_REF_POWMAN);
      /* And on RP2350 the AON tick is XOSC-driven: the SDK re-sources
       * it to LPOSC only on its timer-DORMANT path, so bracket the
       * GPIO one ourselves (the 2.3.0 setters preserve the current
       * time across the switch). */
      powman_timer_set_1khz_tick_source_lposc();
      lposc_switched = true;
#endif
      rc = low_power_dormant_until_gpio_pin_state((uint)pin, edge, high,
                                                  DORMANT_CLOCK_SOURCE_DEFAULT, &keep);
    } else {
      /* Keep the system timer: newlib realtime is
       * get_absolute_time()-based, so Time.now just keeps flowing
       * and no AON involvement is needed on this cell. */
#if defined(PICO_RP2040)
      clock_dest_bitset_add(&keep, CLK_DEST_SYS_TIMER);
#elif defined(PICO_RP2350)
      clock_dest_bitset_add(&keep, CLK_DEST_SYS_TIMER0);
      clock_dest_bitset_add(&keep, CLK_DEST_REF_TICKS);
#endif
      rc = low_power_sleep_until_gpio_pin_state((uint)pin, edge, high, &keep, false);
    }
  }
  result = sleep_result_from_sdk(rc);

  /* Single cleanup: unwind in reverse order of acquisition. */
#if defined(PICO_RP2350)
  if (lposc_switched) {
    powman_timer_set_1khz_tick_source_xosc();
  }
#endif
  if (gpio_suspended) {
    picorb_irq_gpio_resume();
  }
  if (usb_locked) {
    mutex_exit(&usb_mutex);
  }
  if (aon_synced && result == MACHINE_SLEEP_OK) {
    if (!aon_postsync(libc_before_us, aon_before_us)) {
      result = MACHINE_SLEEP_ESTATE;
    }
  }
  if (mask_saved) {
    clocks_hw->sleep_en0 = saved_en0;
    clocks_hw->sleep_en1 = saved_en1;
  }
  if (alarm_stopped) {
    /* Deterministic re-arm, identical for all four cells: in the
     * SLEEP cells the timer ran on with the IRQ masked (stale
     * pending), in the DORMANT cells the timer itself stopped (stale
     * target). Fresh target, cleared pending, then enable. */
    timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + US_PER_MS;
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);
    irq_set_enabled(ALARM_IRQ, true);
  }
  return result;
}

machine_sleep_result_t
Machine_sleep_timer(bool deep, uint32_t ms)
{
  return machine_sleep_run(deep, true, ms, 0, false, false);
}

machine_sleep_result_t
Machine_sleep_gpio(bool deep, int pin, bool edge, bool high)
{
  return machine_sleep_run(deep, false, 0, pin, edge, high);
}

void
Machine_delay_ms(uint32_t ms)
{
  sleep_ms(ms);
}

void
Machine_busy_wait_ms(uint32_t ms)
{
  busy_wait_us_32(1000 * ms);
}

void
Machine_busy_wait_us(uint32_t us)
{
  busy_wait_us_32(us);
}

bool
Machine_get_unique_id(char *id_str)
{
  pico_get_unique_board_id_string(id_str, PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1);
  return true;
}

uint32_t
Machine_stack_usage(void)
{
  // TODO
  return 0;
}

bool
Machine_set_hwclock(const struct timespec *ts)
{
  struct timeval tv;
  tv.tv_sec = ts->tv_sec;
  tv.tv_usec = ts->tv_nsec / 1000; // Convert nanoseconds to microseconds
  settimeofday(&tv, NULL);
  if (aon_timer_is_running()) {
    return aon_timer_set_time(ts);
  }
  return aon_timer_start(ts);
}

bool
Machine_get_hwclock(struct timespec *ts)
{
  return aon_timer_get_time(ts);
}

void
Machine_exit(int status)
{
  (void)status; // no-op
}

uint64_t
Machine_uptime_us(void)
{
  return time_us_64();
}

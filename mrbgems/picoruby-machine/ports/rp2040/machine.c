#include "../../include/hal.h"
#include "../../include/machine.h"
#include "../../include/ringbuffer.h"
#include "../../../picoruby-io-console/include/io-console.h"

#if defined(PICO_RP2040)
  #include "hardware/rosc.h"
  #include "hardware/rtc.h"
#endif
#include "hardware/vreg.h"
#include "hardware/timer.h"
#include "pico/sleep.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "hardware/clocks.h"
#include "hardware/structs/scb.h"
#include "hardware/sync.h"
#include "pico/aon_timer.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <tusb.h>

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

#ifndef PICORUBY_STDIN_BUFFER_SIZE
#define PICORUBY_STDIN_BUFFER_SIZE 256
#endif

static uint8_t stdin_buf_mem[sizeof(RingBuffer) + PICORUBY_STDIN_BUFFER_SIZE]
  __attribute__((aligned(4)));
static RingBuffer *stdin_rb = (RingBuffer *)stdin_buf_mem;

void
hal_stdin_push(uint8_t ch)
{
  /* Only intercept signal chars in cooked mode (like POSIX).
   * In raw mode, pass all bytes through for binary data (e.g. DFU). */
  if (!io_raw_q()) {
    if (ch == 3) {
      sigint_status = MACHINE_SIGINT_RECEIVED;
      return;
    }
    if (ch == 26) {
      sigint_status = MACHINE_SIGTSTP_RECEIVED;
      return;
    }
  }
  RingBuffer_push(stdin_rb, ch);
}

/*-------------------------------------
 *
 * Canonical (cooked mode) line buffer
 *
 *------------------------------------*/

#ifndef PICORUBY_CANONICAL_BUF_SIZE
#define PICORUBY_CANONICAL_BUF_SIZE 256
#endif

static uint8_t canon_buf[PICORUBY_CANONICAL_BUF_SIZE];
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
        hal_write(1, "\b \b", 3);
      }
    }
    return CANON_ACCUMULATING;
  }
  if (raw == '\n' || raw == '\r') {
    if (canon_len < PICORUBY_CANONICAL_BUF_SIZE) {
      canon_buf[canon_len++] = raw;
    }
    if (io_echo_q()) {
      hal_write(1, "\r\n", 2);
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
  if (canon_len < PICORUBY_CANONICAL_BUF_SIZE) {
    canon_buf[canon_len++] = raw;
    if (io_echo_q()) {
      hal_write(1, &raw, 1);
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

static volatile uint32_t interrupt_nesting = 0;

static volatile bool in_tick_processing = false;

#if defined(PICORB_VM_MRUBY)
static mrb_state *mrb_;
#endif

static void
alarm_handler(void)
{
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
  mrb_tick(mrb_);
#elif defined(PICORB_VM_MRUBYC)
  mrbc_tick();
#else
#error "One of PICORB_VM_MRUBY or PICORB_VM_MRUBYC must be defined"
#endif

  __dmb();
  in_tick_processing = false;
}

static void
usb_irq_handler(void)
{
  tud_task();
}

void
#if defined(PICORB_VM_MRUBY)
mrb_hal_task_init(mrb_state *mrb)
#elif defined(PICORB_VM_MRUBYC)
hal_init(void)
#endif
{
#if defined(PICORB_VM_MRUBY)
  mrb_ = (mrb_state *)mrb;
#endif
  RingBuffer_init(stdin_rb, PICORUBY_STDIN_BUFFER_SIZE);
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
  | CLOCKS_SLEEP_EN1_CLK_SYS_USBCTRL_BITS
  | CLOCKS_SLEEP_EN1_CLK_USB_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_UART0_BITS
  | CLOCKS_SLEEP_EN1_CLK_PERI_UART0_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_UART1_BITS
  | CLOCKS_SLEEP_EN1_CLK_PERI_UART1_BITS;
#else
  #error "Unsupported Board"
#endif

  tud_init(TUD_OPT_RHPORT);
  irq_add_shared_handler(USBCTRL_IRQ, usb_irq_handler,
      PICO_SHARED_IRQ_HANDLER_LOWEST_ORDER_PRIORITY);
}

#if defined(PICORB_VM_MRUBY)
void
mrb_hal_task_final(mrb_state *mrb)
{
  (void)mrb;
}
#endif

void
#if defined(PICORB_VM_MRUBYC)
hal_enable_irq(void)
#elif defined(PICORB_VM_MRUBY)
mrb_task_enable_irq(void)
#endif
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
#if defined(PICORB_VM_MRUBYC)
hal_disable_irq(void)
#elif defined(PICORB_VM_MRUBY)
mrb_task_disable_irq(void)
#endif
{
  asm volatile ("cpsid i" : : : "memory");
  __dmb();
  interrupt_nesting++;
}

void
#if defined(PICORB_VM_MRUBYC)
hal_idle_cpu()
#elif defined(PICORB_VM_MRUBY)
mrb_hal_task_idle_cpu(mrb_state *mrb)
#endif
{
#if defined(PICO_RP2040)
  __wfi();
#elif defined(PICO_RP2350)
  /*
   * TODO: Fix this for RP2350
   *       Why does `__wfi` not wake up?
   */
  asm volatile (
    "wfe\n"           // Wait for Event
    "nop\n"           // No Operation
    "sev\n"           // Set Event
    : : : "memory"    // clobber
  );
#endif
}

#if defined(PICORB_VM_MRUBY)
void
mrb_hal_task_sleep_us(mrb_state *mrb, mrb_int usec)
{
  (void)mrb;
  sleep_us((uint32_t)usec);
}
#endif

int
hal_write(int fd, const void *buf, int nbytes)
{
#if CFG_TUD_CDC >= 2
  // Use separate CDC instances: stdout -> CDC0, stderr -> CDC1
  uint8_t cdc_instance = (fd == FD_STDERR) ? CDC_INSTANCE_STDERR : CDC_INSTANCE_STDOUT;
  tud_cdc_n_write(cdc_instance, buf, nbytes);
  int len = tud_cdc_n_write_flush(cdc_instance);
#else
  tud_cdc_write(buf, nbytes);
  int len = tud_cdc_write_flush();
#endif
  return len;
}

int hal_flush(int fd) {
#if CFG_TUD_CDC >= 2
  uint8_t cdc_instance = (fd == FD_STDERR) ? CDC_INSTANCE_STDERR : CDC_INSTANCE_STDOUT;
  int len = tud_cdc_n_write_flush(cdc_instance);
#else
  int len = tud_cdc_write_flush();
#endif
  return len;
}

int
hal_read_available(void)
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
hal_getchar(void)
{
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
      return hal_getchar();
    case CANON_EOF:
      canon_eof = false;
      return HAL_GETCHAR_EOF;
    default:
      /* Still accumulating line */
      return HAL_GETCHAR_NODATA;
  }
}

void
hal_abort(const char *s)
{
  if( s ) {
    hal_write(1, s, strlen(s));
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
  tud_task();
}

bool
Machine_tud_mounted_q(void)
{
  return tud_mounted();
}


/*-------------------------------------
 *
 * Sleep in low power mode
 *
 *------------------------------------*/

#if defined(PICO_RP2040)

static const uint32_t PIN_DCDC_PSM_CTRL = 23;
static uint32_t _scr;
static uint32_t _sleep_en0;
static uint32_t _sleep_en1;
static volatile bool sleep_wakeup_flag = false;

/*
 * deep_sleep doesn't work yet
 */
void
Machine_deep_sleep(uint8_t gpio_pin, bool edge, bool high)
{
  bool psm = gpio_get(PIN_DCDC_PSM_CTRL);
  gpio_put(PIN_DCDC_PSM_CTRL, 0); // PFM mode for better efficiency
  uint32_t ints = save_and_disable_interrupts();
  {
    // preserve_clock_before_sleep
    _scr = scb_hw->scr;
    _sleep_en0 = clocks_hw->sleep_en0;
    _sleep_en1 = clocks_hw->sleep_en1;
  }
  sleep_run_from_xosc();
  sleep_goto_dormant_until_pin(gpio_pin, edge, high);
  {
    // recover_clock_after_sleep
    rosc_write(&rosc_hw->ctrl, ROSC_CTRL_ENABLE_BITS); //Re-enable ring Oscillator control
    scb_hw->scr = _scr;
    clocks_hw->sleep_en0 = _sleep_en0;
    clocks_hw->sleep_en1 = _sleep_en1;
    //clocks_init();
    set_sys_clock_khz(125000, true);
  }
  restore_interrupts(ints);
  gpio_put(PIN_DCDC_PSM_CTRL, psm); // recover PWM mode
}

static void
sleep_callback(uint alarm_num)
{
  // This callback is called when the hardware alarm expires and wakes up the system
  (void)alarm_num; // Suppress unused parameter warning
  sleep_wakeup_flag = true;
}

static void
recover_from_sleep(uint scb_orig, uint clock0_orig, uint clock1_orig)
{
  //Re-enable ring Oscillator control
  rosc_write(&rosc_hw->ctrl, ROSC_CTRL_ENABLE_BITS);
  //reset procs back to default
  scb_hw->scr = scb_orig;
  clocks_hw->sleep_en0 = clock0_orig;
  clocks_hw->sleep_en1 = clock1_orig;
  //reset clocks
  set_sys_clock_khz(125000, true);
  // Re-initialize peripherals
  stdio_init_all();
}

#elif defined(PICO_RP2350)
void
Machine_deep_sleep(uint8_t gpio_pin, bool edge, bool high)
{
}
#endif

void
Machine_sleep(uint32_t seconds)
{
#if defined(PICO_RP2040)
  // RP2040 implementation with deep power saving
  // Reset wakeup flag
  sleep_wakeup_flag = false;

  // Stop system alarm_handler (ALARM_NUM 0) during deep sleep
  irq_set_enabled(ALARM_IRQ, false);

  // Save current system state for recovery
  uint scb_orig = scb_hw->scr;
  uint clock0_orig = clocks_hw->sleep_en0;
  uint clock1_orig = clocks_hw->sleep_en1;

  // Use a fixed alarm number (1) to avoid conflict with hal_init's alarm 0
  int alarm_num = 1;

  // Try to claim alarm 1
  hardware_alarm_claim(alarm_num);

  // Set up the alarm callback
  hardware_alarm_set_callback(alarm_num, sleep_callback);

  // Set target time
  absolute_time_t target = make_timeout_time_ms(seconds * 1000);
  if (hardware_alarm_set_target(alarm_num, target)) {
    // Failed to set target, clean up and fallback
    hardware_alarm_set_callback(alarm_num, NULL);
    hardware_alarm_unclaim(alarm_num);
    sleep_ms(seconds * 1000);
    return;
  }

  // Prepare for deep sleep - switch to low-power clock source
  sleep_run_from_xosc();

  // Configure clocks for maximum power saving
  // Disable all non-essential clocks for deep power saving
  clocks_hw->sleep_en0 = 0x0; // Disable all clocks in sleep_en0

  // Keep only the absolute minimum for wakeup functionality
  clocks_hw->sleep_en1 = CLOCKS_SLEEP_EN1_CLK_SYS_TIMER_BITS; // Timer for wakeup alarm

  // Flush any pending output before sleep
  stdio_flush();

  // Save current VREG setting and switch to power-saving mode
  uint32_t vreg_orig = vreg_get_voltage();
  vreg_set_voltage(VREG_VOLTAGE_1_10); // Reduce voltage for power saving

  // Enable deep sleep mode at the processor level
  scb_hw->scr |= M0PLUS_SCR_SLEEPDEEP_BITS;

  // Enter deep sleep - wait for alarm interrupt
  while (!sleep_wakeup_flag) {
    __wfi();
  }

  // Wakeup - restore system clocks and peripherals
  sleep_power_up();

  // Restore VREG voltage to normal operating level
  vreg_set_voltage(vreg_orig);

  // Wait for voltage to stabilize before continuing
  busy_wait_us(100);

  recover_from_sleep(scb_orig, clock0_orig, clock1_orig);

  // Clean up alarm
  hardware_alarm_set_callback(alarm_num, NULL);
  hardware_alarm_unclaim(alarm_num);

  // Restart system alarm_handler (ALARM_NUM 0) after deep sleep
  irq_set_enabled(ALARM_IRQ, true);
#elif defined(PICO_RP2350)
  // RP2350 implementation using dormant mode with AON timer
  stdio_flush();

  // Stop system alarm_handler (ALARM_NUM 0) during deep sleep
  irq_set_enabled(ALARM_IRQ, false);

  // Get current time from AON timer
  struct timespec current_time;
  if (!aon_timer_get_time(&current_time)) {
    // Fallback to simple sleep if AON timer not available
    sleep_ms(seconds * 1000);
    irq_set_enabled(ALARM_IRQ, true);
    return;
  }

  // Calculate wake time
  struct timespec wake_time;
  wake_time.tv_sec = current_time.tv_sec + seconds;
  wake_time.tv_nsec = current_time.tv_nsec;

  // Save current VREG setting and switch to power-saving mode
  uint32_t vreg_orig = vreg_get_voltage();
  vreg_set_voltage(VREG_VOLTAGE_1_10); // Same voltage as RP2040 for stability

  // Switch to low-power clock source for dormant mode
  sleep_run_from_lposc();

  // Enter dormant mode until specified wake time (with NULL callback)
  sleep_goto_dormant_until(&wake_time, NULL);

  // Restore clocks after wake up
  sleep_power_up();

  // Restore VREG voltage to normal operating level
  vreg_set_voltage(vreg_orig);

  // Wait for voltage to stabilize before continuing
  busy_wait_us(100);

  // Restart system alarm_handler (ALARM_NUM 0) after deep sleep
  irq_set_enabled(ALARM_IRQ, true);
#endif
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

const char *
Machine_mcu_name(void)
{
#if defined(PICO_RP2040)
  return "RP2040";
#elif defined(PICO_RP2350)
  return "RP2350";
#endif
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

#include "../../include/hal.h"

//#if !defined(PICO_RP2350)
#if 0
  #include "pico/sleep.h"
  #include "hardware/rosc.h"
#endif

#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "hardware/clocks.h"
#include "hardware/structs/scb.h"
#include "hardware/sync.h"
#include "pico/util/datetime.h"
#include <tusb.h>

#include "pico/aon_timer.h"

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
hal_init(mrb_state *mrb)
#elif defined(PICORB_VM_MRUBYC)
hal_init(void)
#endif
{
#if defined(PICORB_VM_MRUBY)
  mrb_ = (mrb_state *)mrb;
#endif
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

void
hal_enable_irq()
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
hal_disable_irq()
{
  asm volatile ("cpsid i" : : : "memory");
  __dmb();
  interrupt_nesting++;
}

void
hal_idle_cpu()
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

int
hal_write(int fd, const void *buf, int nbytes)
{
  tud_cdc_write(buf, nbytes);
  int len = tud_cdc_write_flush();
  return len;
}

int hal_flush(int fd) {
  int len = tud_cdc_write_flush();
  return len;
}

int
hal_read_available(void)
{
  int len = tud_cdc_available();
  if (0 < len) {
    return 1;
  } else {
    return 0;
  }
}

int
hal_getchar(void)
{
  int c = -1;
  int len = tud_cdc_available();
  if (0 < len) {
    c = tud_cdc_read_char();
  }
  return c;
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
 * RTC
 *
 *------------------------------------*/

/*
 * References:
 *   https://github.com/ghubcoder/PicoSleepDemo
 *   https://ghubcoder.github.io/posts/awaking-the-pico/
 */

/*
 * Current implementation can not restore USB-CDC.
 * This article might help:
 *   https://elehobica.blogspot.com/2021/08/raspberry-pi-pico-2.html
 */

#define YEAR  2020
#define MONTH    6
#define DAY      5
#define DOTW     5

//#if !defined(PICO_RP2350)
#if 0
  static const uint32_t PIN_DCDC_PSM_CTRL = 23;
  static uint32_t _scr;
  static uint32_t _sleep_en0;
  static uint32_t _sleep_en1;
#endif

/*
 * deep_sleep doesn't work yet
 */
void
Machine_deep_sleep(uint8_t gpio_pin, bool edge, bool high)
{
//#if !defined(PICO_RP2350)
#if 0
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
#endif
}

static void
sleep_callback(void)
{
  return;
}

static void
rtc_sleep(uint32_t seconds)
{
//#if !defined(PICO_RP2350)
#if 0
  datetime_t t;
  rtc_get_datetime(&t);
  t.sec += seconds;
  if (t.sec >= 60) {
    t.min += t.sec / 60;
    t.sec %= 60;
  }
  if (t.min >= 60) {
    t.hour += t.min / 60;
    t.min %= 60;
  }
  sleep_goto_sleep_until(&t, &sleep_callback);
#endif
}


static void
recover_from_sleep(uint scb_orig, uint clock0_orig, uint clock1_orig)
{
//#if !defined(PICO_RP2350)
#if 0
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
#endif
}

void
Machine_sleep(uint32_t seconds)
{
//#if !defined(PICO_RP2350)
#if 0
  // save values for later
  uint scb_orig = scb_hw->scr;
  uint clock0_orig = clocks_hw->sleep_en0;
  uint clock1_orig = clocks_hw->sleep_en1;

  // Start the Real time clock
  rtc_init();
  sleep_run_from_xosc();

  // Get current time and set alarm
  datetime_t t;
  rtc_get_datetime(&t);

  rtc_sleep(seconds);

  // reset processor and clocks back to defaults
  recover_from_sleep(scb_orig, clock0_orig, clock1_orig);

  // システムクロックをリセットし、デフォルト設定に戻す
  clock_configure(clk_sys,
                  CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                  CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_XOSC_CLKSRC,
                  12 * MHZ,  // XOSC frequency
                  12 * MHZ);

  // PLLを使用して通常の動作周波数に戻す
  set_sys_clock_khz(133000, true);  // 133 MHz、デフォルトの周波数

  // Re-enable interrupts
  irq_set_enabled(RTC_IRQ, true);
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

int
Machine_get_unique_id(char *id_str)
{
  pico_get_unique_board_id_string(id_str, PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1);
  return 1;
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

#include "pico/sleep.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "hardware/clocks.h"
#include "hardware/rosc.h"
#include "hardware/structs/scb.h"
#include "hardware/sync.h"

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
static const uint32_t PIN_DCDC_PSM_CTRL = 23;
static uint32_t _scr;
static uint32_t _sleep_en0;
static uint32_t _sleep_en1;

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
sleep_callback(void)
{
  return;
}

static void
rtc_sleep(uint32_t seconds)
{
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

void
Machine_sleep(uint32_t seconds)
{
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

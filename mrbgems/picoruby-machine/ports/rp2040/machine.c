#include "pico/sleep.h"
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "hardware/clocks.h"
#include "hardware/rosc.h"
#include "hardware/structs/scb.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"

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
    clocks_init();
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
  uint8_t hours_to_sleep_to = seconds / 3600;
  uint8_t minute_to_sleep_to = seconds / 60;
  uint8_t second_to_sleep_to = seconds % 60;
  datetime_t t_alarm = {
          .year  = YEAR,
          .month = MONTH,
          .day   = DAY,
          .dotw  = DOTW,
          .hour  = hours_to_sleep_to,
          .min   = minute_to_sleep_to,
          .sec   = second_to_sleep_to
  };

  sleep_goto_sleep_until(&t_alarm, &sleep_callback);
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
  clocks_init();
  stdio_init_all();
  return;
}

void
Machine_sleep(uint32_t seconds)
{
  // save values for later
  uint scb_orig = scb_hw->scr;
  uint clock0_orig = clocks_hw->sleep_en0;
  uint clock1_orig = clocks_hw->sleep_en1;

  // crudely reset the clock each time to the value below
  datetime_t t = {
          .year  = YEAR,
          .month = MONTH,
          .day   = DAY,
          .dotw  = DOTW,
          .hour  = 0,
          .min   = 0,
          .sec   = 0
  };

  // Start the Real time clock
  rtc_init();

  sleep_run_from_xosc();
  // Reset real time clock to a value
  rtc_set_datetime(&t);
  // sleep here, in this case for 1 min
  rtc_sleep(seconds);

  // reset processor and clocks back to defaults
  recover_from_sleep(scb_orig, clock0_orig, clock1_orig);
}

void
Machine_reboot(uint32_t delay_ms)
{
  watchdog_reboot(0, 0, delay_ms);
}

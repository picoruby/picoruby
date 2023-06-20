#include "pico/sleep.h"
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "hardware/clocks.h"
#include "hardware/rosc.h"
#include "hardware/structs/scb.h"

#include "mrubyc.h"

static void
sleep_callback(void)
{
  return;
}

#define YEAR    2020
#define MONTH   1
#define DAY     1

static void
rtc_sleep(uint32_t seconds_to_sleep)
{
  struct tm t = { .tm_year = YEAR - 1900,
                  .tm_mon  = MONTH - 1,
                  .tm_mday = DAY,
                  .tm_hour = 0,
                  .tm_min  = 0,
                  .tm_sec  = 0 };
  t.tm_mday += seconds_to_sleep / (24 * 60 * 60);
  seconds_to_sleep %= 24 * 60 * 60;
  t.tm_hour += seconds_to_sleep / (60 * 60);
  seconds_to_sleep %= 60 * 60;
  t.tm_min += seconds_to_sleep / 60;
  seconds_to_sleep %= 60;
  t.tm_sec += seconds_to_sleep;
  mktime(&t);

  datetime_t t_alarm = {
      .year  = t.tm_year + 1900,
      .month = t.tm_mon + 1,
      .day   = t.tm_mday,
      .dotw  = t.tm_wday, // 0 is Sunday, so 5 is Friday
      .hour  = t.tm_hour,
      .min   = t.tm_min,
      .sec   = t.tm_sec };
  sleep_goto_sleep_until(&t_alarm, &sleep_callback);
}


void
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
          .dotw  = 5, // 0 is Sunday, so 5 is Friday
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
  // will return here
  //reset processor and clocks back to defaults
  recover_from_sleep(scb_orig, clock0_orig, clock1_orig);
}

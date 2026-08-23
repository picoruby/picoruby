#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_rtc.h"

#include "../../include/machine.h"

static void
hex32_to_str(uint32_t value, char *dst)
{
  static const char hex[] = "0123456789ABCDEF";

  for (int i = 0; i < 8; i++) {
    dst[7 - i] = hex[value & 0x0f];
    value >>= 4;
  }
}

bool
Machine_get_unique_id(char *id_str)
{
  hex32_to_str(NRF_FICR->DEVICEID[0], &id_str[0]);
  hex32_to_str(NRF_FICR->DEVICEID[1], &id_str[8]);
  id_str[16] = '\0';
  return true;
}

uint32_t
Machine_stack_usage(void)
{
  return 0;
}

/*
 * Weak, because a product that owns a console has to keep the link
 * alive while it waits. R2P2-nRF's USB CDC build pumps the Nordic
 * app_usbd stack from its own Machine_delay_ms; a busy wait here
 * would freeze the console for the whole delay. So this is the
 * plain-SDK default for a board without one -- picoruby-nRF's bare
 * UARTE build -- and any such product overrides it.
 */
__attribute__((weak)) void
Machine_delay_ms(uint32_t ms)
{
  nrf_delay_ms(ms);
}

/*
 * Weak for symmetry with Machine_delay_ms: a product may need to quiesce
 * peripherals first. The reset itself is the same on every nRF52.
 */
__attribute__((weak)) void
Machine_reboot(void)
{
  NVIC_SystemReset();
}

/*
 * Wall clock on RTC2.
 *
 * nRF52 has no calendar peripheral and no backup domain, so "hardware
 * clock" here means a monotonic tick count with an epoch offset applied.
 * It survives a soft reboot no better than RAM does -- there is nothing
 * on this part that would.
 *
 * RTC2 rather than the alternatives, and the reason is future-proofing
 * as much as availability: RTC0 is reserved by the SoftDevice, which
 * this board will want when BLE arrives, and RTC1 belongs to app_timer.
 * RTC2 is free today and stays free then.
 *
 * Machine_uptime_us is deliberately not used as the time base. On this
 * platform it is derived from app_timer_cnt_get(), which returns the
 * 24-bit RTC counter, so it wraps every 512 seconds -- fine for an
 * uptime readout, useless for a clock that has to keep counting past
 * nine minutes.
 */
#define RTC_TICKS_PER_SECOND 32768u
#define RTC_COUNTER_BITS     24
#define RTC_COUNTER_HALF     (1u << (RTC_COUNTER_BITS - 1))

static volatile uint32_t rtc_overflows;
static bool     clock_started;
static uint64_t epoch_offset_ticks;   /* wall clock at tick 0 */
static bool     epoch_known;

void
RTC2_IRQHandler(void)
{
  if (nrf_rtc_event_pending(NRF_RTC2, NRF_RTC_EVENT_OVERFLOW)) {
    nrf_rtc_event_clear(NRF_RTC2, NRF_RTC_EVENT_OVERFLOW);
    rtc_overflows++;
  }
}

static void
clock_start(void)
{
  if (clock_started) {
    return;
  }

  /* The RTC counts LFCLK, so it has to be running. Started here only if
     nobody else has: the product may already own it for app_timer or the
     USB stack, and restarting a running LFCLK is not free. */
  if (((NRF_CLOCK->LFCLKSTAT & CLOCK_LFCLKSTAT_STATE_Msk) >> CLOCK_LFCLKSTAT_STATE_Pos)
        == CLOCK_LFCLKSTAT_STATE_NotRunning) {
    NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_LFCLKSTART = 1;
    while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0) {
    }
    NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
  }

  nrf_rtc_prescaler_set(NRF_RTC2, 0);   /* 32768 Hz, one tick per LFCLK */
  nrf_rtc_event_clear(NRF_RTC2, NRF_RTC_EVENT_OVERFLOW);
  nrf_rtc_event_enable(NRF_RTC2, RTC_EVTEN_OVRFLW_Msk);
  nrf_rtc_int_enable(NRF_RTC2, NRF_RTC_INT_OVERFLOW_MASK);
  /* Priority 6, as app_timer and the USB driver use: an overflow tick
     must not preempt either. */
  NVIC_SetPriority(RTC2_IRQn, 6);
  NVIC_ClearPendingIRQ(RTC2_IRQn);
  NVIC_EnableIRQ(RTC2_IRQn);
  nrf_rtc_task_trigger(NRF_RTC2, NRF_RTC_TASK_START);

  clock_started = true;
}

/*
 * Overflow count and counter have to agree. Reading them separately
 * leaves two races: the counter wrapping between the two reads, and an
 * overflow that has fired but whose handler has not run yet. The retry
 * closes the first; the pending-event check closes the second, using the
 * counter's half-way point to tell "just wrapped" from "about to".
 */
static uint64_t
ticks_now(void)
{
  uint32_t overflows;
  uint32_t counter;

  do {
    overflows = rtc_overflows;
    counter = nrf_rtc_counter_get(NRF_RTC2);
  } while (overflows != rtc_overflows);

  if (nrf_rtc_event_pending(NRF_RTC2, NRF_RTC_EVENT_OVERFLOW) &&
      counter < RTC_COUNTER_HALF) {
    overflows++;
  }
  return ((uint64_t)overflows << RTC_COUNTER_BITS) | counter;
}

bool
Machine_set_hwclock(const struct timespec *ts)
{
  if (ts == NULL || ts->tv_sec < 0) {
    return false;
  }
  clock_start();

  uint64_t wall = (uint64_t)ts->tv_sec * RTC_TICKS_PER_SECOND +
                  ((uint64_t)ts->tv_nsec * RTC_TICKS_PER_SECOND) / 1000000000u;
  epoch_offset_ticks = wall - ticks_now();
  epoch_known = true;
  return true;
}

bool
Machine_get_hwclock(struct timespec *ts)
{
  if (ts == NULL) {
    return false;
  }
  if (!epoch_known) {
    /* Never set. Saying so beats reporting the epoch as if it were now. */
    ts->tv_sec = 0;
    ts->tv_nsec = 0;
    return false;
  }

  uint64_t wall = epoch_offset_ticks + ticks_now();
  ts->tv_sec = (time_t)(wall / RTC_TICKS_PER_SECOND);
  ts->tv_nsec = (long)(((wall % RTC_TICKS_PER_SECOND) * 1000000000u)
                       / RTC_TICKS_PER_SECOND);
  return true;
}

machine_sleep_result_t
Machine_sleep_timer(bool deep, uint32_t ms)
{
  (void)deep; (void)ms;
  return MACHINE_SLEEP_EUNSUPPORTED;
}

machine_sleep_result_t
Machine_sleep_gpio(bool deep, int pin, bool edge, bool high)
{
  (void)deep; (void)pin; (void)edge; (void)high;
  return MACHINE_SLEEP_EUNSUPPORTED;
}

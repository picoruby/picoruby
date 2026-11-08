#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "nrf.h"
#include "nrf_delay.h"

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

bool
Machine_set_hwclock(const struct timespec *ts)
{
  (void)ts;
  return false;
}

bool
Machine_get_hwclock(struct timespec *ts)
{
  if (ts == NULL) {
    return false;
  }
  ts->tv_sec = 0;
  ts->tv_nsec = 0;
  return false;
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

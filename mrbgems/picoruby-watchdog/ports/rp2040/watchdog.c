#include "hardware/watchdog.h"

void
Watchdog_enable(uint32_t delay_ms, bool pause_on_debug)
{
  watchdog_enable(delay_ms, pause_on_debug);
}

void
Watchdog_disable(void)
{
  // Workaround for watchdog_disable();
  // sdk 1.5.1 does not have watchdog_disable() function
  // TODO: migrate to sdk 2.0.0 that has watchdog_disable()
  hw_clear_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_ENABLE_BITS);
}

void
Watchdog_reboot(uint32_t delay_ms)
{
  watchdog_reboot(0, 0, delay_ms);
}

void
Watchdog_start_tick(uint32_t cycles)
{
  watchdog_start_tick(cycles);
}

void
Watchdog_update(void)
{
  watchdog_update();
}

bool
Watchdog_caused_reboot(void)
{
  return watchdog_caused_reboot();
}

bool
Watchdog_enable_caused_reboot(void)
{
  return watchdog_enable_caused_reboot();
}

uint32_t
Watchdog_get_count(void)
{
  return watchdog_get_count();
}

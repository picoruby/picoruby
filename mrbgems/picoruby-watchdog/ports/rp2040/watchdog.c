#include "hardware/watchdog.h"

void
Watchdog_reboot(uint32_t delay_ms)
{
  watchdog_reboot(0, 0, delay_ms);
}

bool
Watchdog_caused_reboot(void)
{
  return watchdog_caused_reboot();
}

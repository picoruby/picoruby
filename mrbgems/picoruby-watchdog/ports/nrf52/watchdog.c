#include "machine.h"

void
Watchdog_enable(uint32_t delay_ms, bool pause_on_debug)
{
  (void)delay_ms;
  (void)pause_on_debug;
}

void
Watchdog_disable(void)
{
}

void
Watchdog_reboot(uint32_t delay_ms)
{
  Machine_delay_ms(delay_ms);
  Machine_reboot();
}

void
Watchdog_start_tick(uint32_t cycles)
{
  (void)cycles;
}

void
Watchdog_update(void)
{
}

bool
Watchdog_caused_reboot(void)
{
  return false;
}

bool
Watchdog_enable_caused_reboot(void)
{
  return false;
}

uint32_t
Watchdog_get_count(void)
{
  return 0;
}

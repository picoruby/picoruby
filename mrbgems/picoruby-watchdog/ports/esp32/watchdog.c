#include "esp_task_wdt.h"

void
Watchdog_enable(uint32_t delay_ms, bool pause_on_debug)
{
  esp_task_wdt_config_t twdt_config = {
    .timeout_ms = delay_ms,
    .trigger_panic = pause_on_debug,
  };
  esp_task_wdt_init(&twdt_config);
  esp_task_wdt_add(NULL);
}

void
Watchdog_disable(void)
{
  esp_task_wdt_deinit();
}

void
Watchdog_reboot(uint32_t delay_ms)
{
  /* Not implemented */
}

void
Watchdog_start_tick(uint32_t cycles)
{
  /* Not implemented */
}

void
Watchdog_update(void)
{
  esp_task_wdt_reset();
}

bool
Watchdog_caused_reboot(void)
{
  /* Not implemented */
  return false;
}

bool
Watchdog_enable_caused_reboot(void)
{
  /* Not implemented */
  return false;
}

uint32_t
Watchdog_get_count(void)
{
  /* Not implemented */
  return 0;
}

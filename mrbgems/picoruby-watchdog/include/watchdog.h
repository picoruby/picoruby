#ifndef WATCHDOG_DEFINED_H_
#define WATCHDOG_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Statuses for the two calls that can be refused. Returned rather than
 * asserted because what a watchdog can do differs per part: nRF52 has no
 * stop task at all, and its CRV and CONFIG are ignored once the counter
 * runs, so a second enable cannot change the timeout. A port that can
 * honour the request returns WATCHDOG_ERROR_NONE.
 */
typedef enum {
  WATCHDOG_ERROR_NONE          =  0,
  WATCHDOG_ERROR_UNSUPPORTED   = -1,  /* the part cannot do this at all */
  WATCHDOG_ERROR_INVALID_VALUE = -2,  /* outside what the counter accepts */
  WATCHDOG_ERROR_ALREADY_ON    = -3,  /* running, and not reconfigurable */
} watchdog_status_t;

int Watchdog_enable(uint32_t delay_ms, bool pause_on_debug);
int Watchdog_disable(void);
void Watchdog_reboot(uint32_t delay_ms);
void Watchdog_start_tick(uint32_t cycles);
void Watchdog_update(void);
bool Watchdog_caused_reboot(void);
bool Watchdog_enable_caused_reboot(void);
uint32_t Watchdog_get_count(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCHDOG_DEFINED_H_ */

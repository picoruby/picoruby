#ifndef WATCHDOG_DEFINED_H_
#define WATCHDOG_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void Watchdog_enable(uint32_t delay_ms, bool pause_on_debug);
void Watchdog_disable(void);
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

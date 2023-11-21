#ifndef WATCHDOG_DEFINED_H_
#define WATCHDOG_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void Watchdog_reboot(uint32_t delay_ms);
bool Watchdog_caused_reboot(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCHDOG_DEFINED_H_ */



#ifndef MACHINE_DEFINED_H_
#define MACHINE_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

void Machine_sleep(uint32_t seconds);
void Machine_deep_sleep(uint8_t gpio_pin, bool edge, bool high);
void Machine_delay_ms(uint32_t ms);
void Machine_busy_wait_ms(uint32_t ms);
int Machine_get_unique_id(char *id_str);
void Machine_tud_task(void);
bool Machine_tud_mounted_q(void);
uint32_t Machine_stack_usage(void);
const char* Machine_mcu_name(void);
bool Machine_set_hwclock(const struct timespec *ts);
bool Machine_get_hwclock(struct timespec *ts);

#ifdef __cplusplus
}
#endif

#endif /* MACHINE_DEFINED_H_ */



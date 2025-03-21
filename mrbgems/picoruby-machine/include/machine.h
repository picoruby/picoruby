#ifndef MACHINE_DEFINED_H_
#define MACHINE_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

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

#ifdef __cplusplus
}
#endif

#endif /* MACHINE_DEFINED_H_ */



#ifndef PWM_DEFINED_H_
#define PWM_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IOError mrbc_define_class(0, "IOError", mrbc_class_object)

uint32_t PWM_init(uint32_t gpio);
void PWM_set_frequency_and_duty(uint32_t slice_num, float frequency, float duty_cycle);
void PWM_set_enabled(uint32_t slice_num, bool enabled);

#ifdef __cplusplus
}
#endif

#endif /* PWM_DEFINED_H_ */


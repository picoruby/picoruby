#ifndef PWM_DEFINED_H_
#define PWM_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>
#include "picoruby.h"

#ifdef __cplusplus
extern "C" {
#endif

void PWM_init(uint32_t pin);
void PWM_set_frequency_and_duty(uint32_t slice_num, picorb_float_t frequency, picorb_float_t duty_cycle);
void PWM_set_enabled(uint32_t slice_num, bool enabled);

#ifdef __cplusplus
}
#endif

#endif /* PWM_DEFINED_H_ */


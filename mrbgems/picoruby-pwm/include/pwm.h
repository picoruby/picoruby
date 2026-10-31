#ifndef PWM_DEFINED_H_
#define PWM_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>
#include "picoruby.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Statuses for the one call that can be refused. Every port has a limit
 * on how many pins it can drive -- four PWM instances on nRF52, eight
 * LEDC channels on esp32 -- and a caller that asks for one too many
 * needs to hear about it rather than get an object whose duty setter
 * does nothing.
 */
typedef enum {
  PWM_ERROR_NONE        =  0,
  PWM_ERROR_INVALID_PIN = -1,  /* out of range, or not PWM-capable */
  PWM_ERROR_NO_INSTANCE = -2,  /* every instance or channel is taken */
} pwm_status_t;

int PWM_init(uint32_t pin);
void PWM_set_frequency_and_duty(uint32_t slice_num, picorb_float_t frequency, picorb_float_t duty_cycle);
void PWM_set_enabled(uint32_t slice_num, bool enabled);

#ifdef __cplusplus
}
#endif

#endif /* PWM_DEFINED_H_ */


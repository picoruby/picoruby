#include "../../include/pwm.h"

int
PWM_init(uint32_t pin)
{
  (void)pin;
  return PWM_ERROR_NONE;
}

void
PWM_set_frequency_and_duty(uint32_t pin, picorb_float_t frequency, picorb_float_t duty_cycle)
{
}

void
PWM_set_enabled(uint32_t pin, bool enabled)
{
}


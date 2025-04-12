#include "pico/stdlib.h"
#include "hardware/pwm.h"

#include "../../include/pwm.h"

#define APB_CLK_FREQ 125000000
#define CLK_DIV      100.0

void
PWM_init(uint32_t gpio)
{
  gpio_set_function(gpio, GPIO_FUNC_PWM);
  uint slice_num = pwm_gpio_to_slice_num(gpio);
  pwm_set_clkdiv(slice_num, CLK_DIV);
}

/*
 * @frequency: in Hz
 * @duty_cycle: in percentage
 */ 
void
PWM_set_frequency_and_duty(uint32_t gpio, picorb_float_t frequency, picorb_float_t duty_cycle)
{
  uint slice_num = pwm_gpio_to_slice_num(gpio);
  uint channel = pwm_gpio_to_channel(gpio);
  picorb_float_t period = 1.0f / frequency;
  uint16_t wrap = (uint16_t)(period * APB_CLK_FREQ / CLK_DIV);
  pwm_set_wrap(slice_num, wrap);
  uint16_t duty = (uint16_t)(wrap * duty_cycle / 100.0f);
  pwm_set_chan_level(slice_num, channel, duty);
}

void
PWM_set_enabled(uint32_t gpio, bool enabled)
{
  uint slice_num = pwm_gpio_to_slice_num(gpio);
  pwm_set_enabled(slice_num, enabled);
}

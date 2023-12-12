#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

#include "../../include/pwm.h"

#define APB_CLK_FREQ 125000000

uint32_t
PWM_init(uint32_t gpio)
{
  gpio_set_function(gpio, GPIO_FUNC_PWM);
  return (uint32_t)pwm_gpio_to_slice_num(gpio);
}

void
PWM_set_frequency_and_duty(uint32_t slice_num, float frequency, float duty_cycle)
{
  // PWMの周期を計算して設定
  float period = 1.0 / frequency;
  uint16_t wrap = (uint16_t)(period * (uint32_t)(APB_CLK_FREQ / 65536.0));

  pwm_set_wrap(slice_num, wrap);

  // PWMのクロックディバイダーを設定（必要に応じて調整）
  pwm_set_clkdiv(slice_num, 1.0);

  // デューティ比を計算して設定
  uint16_t duty = (uint16_t)(wrap * duty_cycle);
  pwm_set_chan_level(slice_num, PWM_CHAN_A, duty); // チャンネル0にデューティ比を設定
}

void
PWM_set_enabled(uint32_t slice_num, bool enabled)
{
  pwm_set_enabled(slice_num, enabled);
}

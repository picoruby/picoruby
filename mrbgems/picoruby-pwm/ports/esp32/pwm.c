#include "driver/ledc.h"

#include "../../include/pwm.h"

static int8_t channel_for_gpio[GPIO_NUM_MAX];
static int next_free_channel = 0;

void
PWM_init(uint32_t gpio)
{
  if (gpio >= GPIO_NUM_MAX) return;
  if (next_free_channel >= LEDC_CHANNEL_MAX) return;

  ledc_timer_config_t ledc_timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .timer_num = LEDC_TIMER_0,
    .duty_resolution = LEDC_TIMER_14_BIT,
    .freq_hz = 1000,
    .clk_cfg = LEDC_AUTO_CLK
  };
  ledc_timer_config(&ledc_timer);

  ledc_channel_config_t ledc_channel = {
    .gpio_num = gpio,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = next_free_channel,
    .timer_sel = LEDC_TIMER_0,
    .intr_type = LEDC_INTR_DISABLE,
    .duty = 0,
    .hpoint = 0
  };
  ledc_channel_config(&ledc_channel);

  channel_for_gpio[gpio] = next_free_channel++;
}

void
PWM_set_frequency_and_duty(uint32_t gpio, picorb_float_t frequency, picorb_float_t duty_cycle)
{
  if (gpio >= GPIO_NUM_MAX) return;

  ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, (uint32_t)frequency);

  int8_t channel = channel_for_gpio[gpio];
  uint32_t max_duty = (1 << LEDC_TIMER_14_BIT) - 1;
  uint32_t duty = (uint32_t)((duty_cycle * max_duty) / 100.0f);

  ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
}

void
PWM_set_enabled(uint32_t gpio, bool enabled)
{
  int8_t channel = channel_for_gpio[gpio];

  if (!enabled) {
    ledc_stop(LEDC_LOW_SPEED_MODE, channel, 0);
  }
}

#include "pico/stdlib.h"
#include "hardware/pwm.h"

#include "../../include/psg.h"

static uint l_gpio;
static uint r_gpio;

static void
psg_pwm_init(uint8_t lpin, uint8_t rpin)
{
  l_gpio = (uint)lpin;
  r_gpio = (uint)rpin;

  gpio_set_function(l_gpio, GPIO_FUNC_PWM);
  gpio_set_function(r_gpio, GPIO_FUNC_PWM);

  uint slice_l = pwm_gpio_to_slice_num(l_gpio);
  uint slice_r = pwm_gpio_to_slice_num(r_gpio);

  /* common settings ------------------------------------------------ */
  pwm_set_wrap(slice_l, MAX_SAMPLE_WIDTH);
  if (slice_l != slice_r)
    pwm_set_wrap(slice_r, MAX_SAMPLE_WIDTH);

  /* optional: div = 1.0 (125 MHz) -> 30.5 kHz carrier @ wrap 4095   */
  pwm_set_clkdiv_int_frac(slice_l, 1, 0);
  if (slice_l != slice_r)
    pwm_set_clkdiv_int_frac(slice_r, 1, 0);

  /* start with zero level to avoid pop                               */
  pwm_set_gpio_level(l_gpio, 0);
  pwm_set_gpio_level(r_gpio, 0);

  pwm_set_enabled(slice_l, true);
  if (slice_l != slice_r)
    pwm_set_enabled(slice_r, true);
}

static void
psg_pwm_start(void (*unused_irq_handler)(void))
{
}

static void
psg_pwm_stop(void)
{
}

static void
psg_pwm_write(uint16_t l, uint16_t r)
{
  pwm_set_gpio_level(l_gpio, l);
  pwm_set_gpio_level(r_gpio, r);
}

const psg_output_api_t psg_drv_pwm = {
  .init  = psg_pwm_init,
  .start = psg_pwm_start,
  .stop  = psg_pwm_stop,
  .write = psg_pwm_write
};

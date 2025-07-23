#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"

#include "../../include/psg.h"

static uint slice_num;
static uint chan_l;
static uint chan_r;

static void
psg_pwm_init(uint8_t lpin, uint8_t rpin)
{
  uint l_gpio = (uint)lpin;
  uint r_gpio = (uint)rpin;

  gpio_set_function(l_gpio, GPIO_FUNC_PWM);
  gpio_set_function(r_gpio, GPIO_FUNC_PWM);

  slice_num = pwm_gpio_to_slice_num(l_gpio);
  if (slice_num != pwm_gpio_to_slice_num(r_gpio)) {
    // ToDo: Should raise an exception
    return;
  }
  chan_l = pwm_gpio_to_channel(l_gpio);
  chan_r = pwm_gpio_to_channel(r_gpio);

  /* common settings ------------------------------------------------ */
  pwm_set_wrap(slice_num, MAX_SAMPLE_WIDTH);

  /* optional: div = 1.0 (125 MHz) -> 30.5 kHz carrier @ wrap 4095   */
  pwm_set_clkdiv_int_frac(slice_num, 1, 0);

  /* start with zero level to avoid pop                               */
  pwm_set_both_levels(slice_num, 0, 0);

  pwm_set_enabled(slice_num, true);
}

static void
psg_pwm_start(void)
{
}

static void
psg_pwm_stop(void)
{
}

static void
psg_pwm_write(uint16_t l, uint16_t r)
{
  if (chan_l == PWM_CHAN_A) {
    pwm_set_both_levels(slice_num, l, r);
  } else {
    pwm_set_both_levels(slice_num, r, l);
  }
}

const psg_output_api_t psg_drv_pwm = {
  .init  = psg_pwm_init,
  .start = psg_pwm_start,
  .stop  = psg_pwm_stop,
  .write = psg_pwm_write
};

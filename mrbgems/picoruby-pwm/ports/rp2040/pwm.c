#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"

#include "../../include/pwm.h"

/* The counter must fit in 16 bits and the divider is 8.4 fixed point
 * (1.0 .. 255.9375). Choosing the divider per frequency instead of
 * fixing it keeps the duty resolution as fine as the hardware allows
 * at that frequency -- a fixed divider of 100 left only twelve steps
 * at 100 kHz -- and extends the reachable range down to
 * sys_clk / 255 / 65536, about 7.5 Hz at 125 MHz. */
#define PWM_WRAP_MAX 65535.0f
#define PWM_DIV_MAX  255.0f

void
PWM_init(uint32_t pin)
{
  gpio_set_function(pin, GPIO_FUNC_PWM);
}

/*
 * @frequency: in Hz
 * @duty_cycle: in percentage
 */
void
PWM_set_frequency_and_duty(uint32_t pin, picorb_float_t frequency, picorb_float_t duty_cycle)
{
  /* Zero is how a caller says "stop", and PWM_set_enabled is what
   * performs it. Dividing by it here would produce an infinity, and
   * casting an infinity to uint16_t is undefined behaviour. */
  if (frequency <= 0.0f) {
    return;
  }
  uint slice_num = pwm_gpio_to_slice_num(pin);
  uint channel = pwm_gpio_to_channel(pin);
  /* The system clock is read, not assumed: 125 MHz on the RP2040,
   * 150 MHz on the RP2350, and either can be changed at runtime. */
  float sys_clk = (float)clock_get_hz(clk_sys);
  float div = sys_clk / ((float)frequency * (PWM_WRAP_MAX + 1.0f));
  if (div < 1.0f) {
    div = 1.0f;
  } else if (PWM_DIV_MAX < div) {
    div = PWM_DIV_MAX;
  }
  float count = sys_clk / (div * (float)frequency);
  if (PWM_WRAP_MAX < count) {
    count = PWM_WRAP_MAX;   /* below the lowest frequency the slice can make */
  } else if (count < 1.0f) {
    count = 1.0f;
  }
  uint16_t wrap = (uint16_t)count;
  pwm_set_clkdiv(slice_num, div);
  pwm_set_wrap(slice_num, wrap);
  uint16_t duty = (uint16_t)((float)wrap * (float)duty_cycle / 100.0f);
  pwm_set_chan_level(slice_num, channel, duty);
}

void
PWM_set_enabled(uint32_t pin, bool enabled)
{
  uint slice_num = pwm_gpio_to_slice_num(pin);
  pwm_set_enabled(slice_num, enabled);
}

/*
 * nRF52 port.
 *
 * One PWM instance per pin, so at most four PWM pins. That looks
 * wasteful -- each instance drives four channels -- but the API sets a
 * frequency per pin, and on nRF52 PRESCALER and COUNTERTOP belong to
 * the instance, not the channel. Sharing an instance between two pins
 * would mean the second PWM.new(...).frequency = call silently moved
 * the first pin's frequency too. Four honest outputs beat sixteen
 * surprising ones; a caller who needs more can be given a grouping API
 * later, when there is something to group by.
 *
 * The parameter named slice_num throughout picoruby-pwm is a pin
 * number -- RP2040 vocabulary that the rp2040 and esp32 ports also
 * take as a pin. Named accordingly here.
 */

#include <stdint.h>
#include <stdbool.h>

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_pwm.h"

#include "../../include/pwm.h"

#define PWM_INSTANCE_COUNT   PWM_COUNT
#define PWM_BASE_CLOCK_HZ    16000000u
/* COUNTERTOP is 15-bit and the hardware rejects values below 3. */
#define PWM_COUNTERTOP_MAX   32767u
#define PWM_COUNTERTOP_MIN   3u
#define PWM_PRESCALER_MAX    7u          /* DIV_128 */
#define PWM_POLARITY_FALLING 0x8000u     /* bit 15 of a Common-load value */

static NRF_PWM_Type *const instances[PWM_INSTANCE_COUNT] = {
  NRF_PWM0, NRF_PWM1, NRF_PWM2, NRF_PWM3
};

/* EasyDMA reads the sequence out of RAM, so the duty value has to
   outlive the call that sets it -- one live cell per instance. */
/* EasyDMA reads this while the sequence plays, so the CPU is not its
   only accessor. */
static volatile uint16_t duty_value[PWM_INSTANCE_COUNT];
static uint32_t   assigned_pin[PWM_INSTANCE_COUNT];
static bool       in_use[PWM_INSTANCE_COUNT];
static uint16_t   countertop[PWM_INSTANCE_COUNT];
static uint8_t    prescaler_of[PWM_INSTANCE_COUNT];

static int
index_of_pin(uint32_t pin)
{
  for (int i = 0; i < PWM_INSTANCE_COUNT; i++) {
    if (in_use[i] && assigned_pin[i] == pin) {
      return i;
    }
  }
  return -1;
}

void
PWM_init(uint32_t pin)
{
  if (pin >= NUMBER_OF_PINS) {
    return;
  }
  if (0 <= index_of_pin(pin)) {
    return;  /* already bound to an instance */
  }

  int idx = -1;
  for (int i = 0; i < PWM_INSTANCE_COUNT; i++) {
    if (!in_use[i]) {
      idx = i;
      break;
    }
  }
  if (idx < 0) {
    return;  /* all four instances are taken */
  }

  NRF_PWM_Type *pwm = instances[idx];

  /* Drive low until a duty arrives, so enabling PWM on a pin cannot
     glitch it high. */
  nrf_gpio_pin_clear(pin);
  nrf_gpio_cfg_output(pin);

  assigned_pin[idx] = pin;
  in_use[idx] = true;
  duty_value[idx] = 0;
  countertop[idx] = PWM_COUNTERTOP_MAX;
  prescaler_of[idx] = 0;

  nrf_pwm_pins_set(pwm, (uint32_t[NRF_PWM_CHANNEL_COUNT]){
    pin, NRF_PWM_PIN_NOT_CONNECTED,
    NRF_PWM_PIN_NOT_CONNECTED, NRF_PWM_PIN_NOT_CONNECTED
  });
  nrf_pwm_configure(pwm, NRF_PWM_CLK_16MHz, NRF_PWM_MODE_UP, countertop[idx]);
  nrf_pwm_decoder_set(pwm, NRF_PWM_LOAD_COMMON, NRF_PWM_STEP_AUTO);
  nrf_pwm_loop_set(pwm, 1);
  /* Restart the sequence as soon as it finishes: the output has to
     keep running without the CPU touching it again. */
  nrf_pwm_shorts_set(pwm, NRF_PWM_SHORT_LOOPSDONE_SEQSTART0_MASK);
}

/*
 * @frequency: in Hz
 * @duty_cycle: in percentage
 */
void
PWM_set_frequency_and_duty(uint32_t pin, picorb_float_t frequency, picorb_float_t duty_cycle)
{
  int idx = index_of_pin(pin);
  if (idx < 0) {
    return;
  }
  /* Zero is how a caller says "stop", and PWM_set_enabled performs it.
     Dividing by it here would produce an infinity, and casting an
     infinity to an integer is undefined behaviour. */
  if (frequency <= 0) {
    return;
  }

  /* Take the fastest clock whose COUNTERTOP still fits: that keeps the
     duty resolution as fine as the hardware allows at this frequency. */
  uint32_t prescaler = 0;
  uint32_t top = 0;
  while (prescaler <= PWM_PRESCALER_MAX) {
    picorb_float_t candidate =
      (picorb_float_t)(PWM_BASE_CLOCK_HZ >> prescaler) / frequency;
    if (candidate <= (picorb_float_t)PWM_COUNTERTOP_MAX) {
      top = (uint32_t)candidate;
      break;
    }
    prescaler++;
  }
  if (prescaler > PWM_PRESCALER_MAX) {
    /* Below the slowest frequency the instance can make: 125 kHz/32767,
       about 3.8 Hz. Clamp rather than fold over. */
    prescaler = PWM_PRESCALER_MAX;
    top = PWM_COUNTERTOP_MAX;
  }
  if (top < PWM_COUNTERTOP_MIN) {
    top = PWM_COUNTERTOP_MIN;
  }

  if (duty_cycle < 0) {
    duty_cycle = 0;
  } else if (100 < duty_cycle) {
    duty_cycle = 100;
  }

  NRF_PWM_Type *pwm = instances[idx];
  /*
   * Only touch PRESCALER/COUNTERTOP when the frequency actually moved.
   * They are init-time registers as far as nrfx is concerned, and
   * rewriting them under a running sequence stretches or truncates the
   * period in progress. A duty-only change needs none of it: the
   * sequence value is re-read every period, which is the one update the
   * hardware documents as safe.
   */
  if (countertop[idx] != (uint16_t)top || prescaler_of[idx] != (uint8_t)prescaler) {
    countertop[idx] = (uint16_t)top;
    prescaler_of[idx] = (uint8_t)prescaler;
    nrf_pwm_configure(pwm, (nrf_pwm_clk_t)prescaler, NRF_PWM_MODE_UP, countertop[idx]);
  }

  uint32_t compare = (uint32_t)(((picorb_float_t)top * duty_cycle) / 100);
  if (top < compare) {
    compare = top;
  }
  /* Polarity bit clear would invert this: with it set the channel is
     high from the start of the period until the compare. */
  duty_value[idx] = (uint16_t)compare | PWM_POLARITY_FALLING;

  nrf_pwm_sequence_t const seq = {
    .values     = { .p_common = (uint16_t *)&duty_value[idx] },
    .length     = 1,
    .repeats    = 0,
    .end_delay  = 0,
  };
  /*
   * Both sequences, pointing at the same value. LOOP counts completed
   * SEQ0+SEQ1 pairs, and nrf_pwm_loop_set's own documentation says a
   * single sequence can be played back only once -- leaving SEQ1 at its
   * reset value would play one period and then walk into an empty
   * sequence whose PTR is 0, which is not even Data RAM. nrfx does the
   * same thing for the same reason (nrfx_pwm.c, simple_playback).
   */
  nrf_pwm_sequence_set(pwm, 0, &seq);
  nrf_pwm_sequence_set(pwm, 1, &seq);
  nrf_pwm_task_trigger(pwm, NRF_PWM_TASK_SEQSTART0);
}

void
PWM_set_enabled(uint32_t pin, bool enabled)
{
  int idx = index_of_pin(pin);
  if (idx < 0) {
    return;
  }

  NRF_PWM_Type *pwm = instances[idx];
  if (enabled) {
    nrf_pwm_enable(pwm);
    nrf_pwm_task_trigger(pwm, NRF_PWM_TASK_SEQSTART0);
  } else {
    /* STOP alone leaves the output wherever the period happened to be.
       Disable and drive the pin low so "off" means off. */
    nrf_pwm_task_trigger(pwm, NRF_PWM_TASK_STOP);
    nrf_pwm_disable(pwm);
    nrf_gpio_pin_clear(assigned_pin[idx]);
    nrf_gpio_cfg_output(assigned_pin[idx]);
  }
}

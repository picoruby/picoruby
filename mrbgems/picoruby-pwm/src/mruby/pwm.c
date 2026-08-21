#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/variable.h>

static mrb_value
mrb__init(mrb_state *mrb, mrb_value self)
{
  mrb_int pin;
  mrb_get_args(mrb, "i", &pin);
  PWM_init(pin);
  return mrb_fixnum_value(0);
}

static void
set_freq_and_start(mrb_state *mrb, mrb_value self, picorb_float_t freq)
{
  mrb_iv_set(mrb, self, MRB_IVSYM(frequency), mrb_float_value(mrb, freq));
  uint32_t pin = mrb_fixnum(mrb_iv_get(mrb, self, MRB_IVSYM(pin)));
  picorb_float_t duty = mrb_float(mrb_iv_get(mrb, self, MRB_IVSYM(duty)));
  /* Zero means stop: the settings are not pushed at all, or the port
   * would have to divide by it. */
  if (0 < freq) {
    PWM_set_frequency_and_duty(pin, freq, duty);
    PWM_set_enabled(pin, true);
  } else {
    PWM_set_enabled(pin, false);
  }
}

static mrb_value
mrb_frequency(mrb_state *mrb, mrb_value self)
{

  mrb_value freq;
  mrb_get_args(mrb, "o", &freq);
  if (mrb_float_p(freq)) {
    set_freq_and_start(mrb, self, mrb_float(freq));
    return freq;
  } else if (mrb_fixnum_p(freq)) {
    set_freq_and_start(mrb, self, (picorb_float_t)mrb_fixnum(freq));
    return mrb_float_value(mrb, (picorb_float_t)mrb_fixnum(freq));
  } else {
    mrb_raise(mrb, E_TYPE_ERROR, "wrong argument type");
  }
}

static mrb_value
mrb_period_us(mrb_state *mrb, mrb_value self)
{
  mrb_int period_us;
  mrb_get_args(mrb, "i", &period_us);
  if (period_us <= 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "period must be positive");
  }
  picorb_float_t freq = 1000000.0 / period_us;
  set_freq_and_start(mrb, self, freq);
  return mrb_float_value(mrb, freq);
}

/* returns the duty actually set: the clamped one, which is what the
 * mruby/c binding has always returned */
static picorb_float_t
set_duty(mrb_state *mrb, mrb_value self, picorb_float_t duty)
{
  if (duty < 0.0) {
    duty = 0.0;
  } else if (100.0 < duty) {
    duty = 100.0;
  }
  mrb_iv_set(mrb, self, MRB_IVSYM(duty), mrb_float_value(mrb, duty));
  mrb_value pin = mrb_iv_get(mrb, self, MRB_IVSYM(pin));
  picorb_float_t freq = mrb_float(mrb_iv_get(mrb, self, MRB_IVSYM(frequency)));
  if (0 < freq) {
    PWM_set_frequency_and_duty(mrb_fixnum(pin), freq, duty);
  }
  return duty;
}

static mrb_value
mrb_duty(mrb_state *mrb, mrb_value self)
{
  mrb_value duty;
  mrb_get_args(mrb, "o", &duty);
  if (mrb_float_p(duty)) {
    return mrb_float_value(mrb, set_duty(mrb, self, mrb_float(duty)));
  } else if (mrb_fixnum_p(duty)) {
    return mrb_float_value(mrb,
                           set_duty(mrb, self, (picorb_float_t)mrb_fixnum(duty)));
  } else {
    mrb_raise(mrb, E_TYPE_ERROR, "wrong argument type");
  }
}

static mrb_value
mrb_pulse_width_us(mrb_state *mrb, mrb_value self)
{
  mrb_int pulse_width;
  mrb_get_args(mrb, "i", &pulse_width);
  mrb_value frequency = mrb_iv_get(mrb, self, MRB_IVSYM(frequency));
  picorb_float_t duty = (picorb_float_t)pulse_width / 10000.0 * mrb_float(frequency);
  return mrb_float_value(mrb, set_duty(mrb, self, duty));
}

void
mrb_picoruby_pwm_gem_init(mrb_state* mrb)
{
  struct RClass *class_PWM = mrb_define_class_id(mrb, MRB_SYM(PWM), mrb->object_class);

  mrb_define_method_id(mrb, class_PWM, MRB_SYM(_init), mrb__init, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_PWM, MRB_SYM(frequency), mrb_frequency, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_PWM, MRB_SYM(period_us), mrb_period_us, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_PWM, MRB_SYM(duty), mrb_duty, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_PWM, MRB_SYM(pulse_width_us), mrb_pulse_width_us, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_pwm_gem_final(mrb_state* mrb)
{
}

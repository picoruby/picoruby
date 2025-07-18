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
  uint32_t gpio = mrb_fixnum(mrb_iv_get(mrb, self, MRB_IVSYM(gpio)));
  picorb_float_t duty = mrb_float(mrb_iv_get(mrb, self, MRB_IVSYM(duty)));
  PWM_set_frequency_and_duty(gpio, freq, duty);
  if (0 < freq) {
    PWM_set_enabled(gpio, true);
  } else {
    PWM_set_enabled(gpio, false);
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
  picorb_float_t freq = 1000000.0 / period_us;
  set_freq_and_start(mrb, self, freq);
  return mrb_float_value(mrb, freq);
}

static void
set_duty(mrb_state *mrb, mrb_value self, picorb_float_t duty)
{
  if (duty < 0.0) {
    duty = 0.0;
  } else if (100.0 < duty) {
    duty = 100.0;
  }
  mrb_iv_set(mrb, self, MRB_IVSYM(duty), mrb_float_value(mrb, duty));
  mrb_value gpio = mrb_iv_get(mrb, self, MRB_IVSYM(gpio));
  mrb_value frequency = mrb_iv_get(mrb, self, MRB_IVSYM(frequency));
  PWM_set_frequency_and_duty(mrb_fixnum(gpio), mrb_float(frequency), duty);
}

static mrb_value
mrb_duty(mrb_state *mrb, mrb_value self)
{
  mrb_value duty;
  mrb_get_args(mrb, "o", &duty);
  if (mrb_float_p(duty)) {
    set_duty(mrb, self, mrb_float(duty));
    return duty;
  } else if (mrb_fixnum_p(duty)) {
    set_duty(mrb, self, (picorb_float_t)mrb_fixnum(duty));
    return mrb_float_value(mrb, (picorb_float_t)mrb_fixnum(duty));
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
  set_duty(mrb, self, duty);
  return mrb_float_value(mrb, duty);
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

#include <string.h>
#include <mrubyc.h>
#include "../include/pwm.h"

#define GETIV(str)  mrbc_instance_getiv(&v[0], mrbc_str_to_symid(#str))
#define SETIV(str, val) mrbc_instance_setiv(&v[0], mrbc_str_to_symid(#str), val)

static void
c__init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  PWM_init(GET_INT_ARG(1));
  SET_INT_RETURN(0);
}

static void
set_freq_and_start(mrbc_value *v, mrbc_float_t freq)
{
  SETIV(frequency, &mrbc_float_value(vm, freq));
  uint32_t gpio = GETIV(gpio).i;
  mrbc_float_t duty = GETIV(duty).d;
  PWM_set_frequency_and_duty(gpio, freq, duty);
  if (0 < freq) {
    PWM_set_enabled(gpio, true);
  } else {
    PWM_set_enabled(gpio, false);
  }
  SET_FLOAT_RETURN(freq);
}

static void
c_frequency(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_float_t freq;
  if (GET_TT_ARG(1) == MRBC_TT_FLOAT) {
    freq = GET_FLOAT_ARG(1);
  } else if (GET_TT_ARG(1) == MRBC_TT_INTEGER) {
    freq = (mrbc_float_t)GET_INT_ARG(1);
  } else {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong argument type");
    return;
  }
  set_freq_and_start(v, freq);
}

static void
c_period_us(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (GET_TT_ARG(1) != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong argument type");
    return;
  }
  mrbc_float_t freq = 1000000.0 / GET_INT_ARG(1);
  set_freq_and_start(v, freq);
}

static void
set_duty(mrbc_value *v, mrbc_float_t duty)
{
  if (duty < 0.0) {
    duty = 0.0;
  } else if (100.0 < duty) {
    duty = 100.0;
  }
  SETIV(duty, &mrbc_float_value(vm, duty));
  PWM_set_frequency_and_duty(GETIV(gpio).i, GETIV(frequency).d, duty);
  SET_FLOAT_RETURN(duty);
}

static void
c_duty(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_float_t duty;
  if (GET_TT_ARG(1) == MRBC_TT_FLOAT) {
    duty = GET_FLOAT_ARG(1);
  } else if (GET_TT_ARG(1) == MRBC_TT_INTEGER) {
    duty = (mrbc_float_t)GET_INT_ARG(1);
  } else {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong argument type");
    return;
  }
  set_duty(v, duty);
}

static void
c_pulse_width_us(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (GET_TT_ARG(1) != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong argument type");
    return;
  }
  mrbc_float_t duty = (mrbc_float_t)GET_INT_ARG(1) / 10000.0 * GETIV(frequency).d;
  set_duty(v, duty);
}

void
mrbc_pwm_init(void)
{
  mrbc_class *mrbc_class_PWM = mrbc_define_class(0, "PWM", mrbc_class_object);
  mrbc_define_method(0, mrbc_class_PWM, "_init", c__init);
  mrbc_define_method(0, mrbc_class_PWM, "frequency", c_frequency);
  mrbc_define_method(0, mrbc_class_PWM, "period_us", c_period_us);
  mrbc_define_method(0, mrbc_class_PWM, "duty", c_duty);
  mrbc_define_method(0, mrbc_class_PWM, "pulse_width_us", c_pulse_width_us);
}

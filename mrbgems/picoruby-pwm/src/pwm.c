#include <string.h>
#include <mrubyc.h>
#include "../include/pwm.h"

static void
c__init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SET_INT_RETURN((uint32_t)PWM_init(GET_INT_ARG(1)));
}

static void
c__set_frequency_and_duty(mrbc_vm *vm, mrbc_value *v, int argc)
{
  PWM_set_frequency_and_duty(GET_INT_ARG(1), GET_FLOAT_ARG(2), GET_FLOAT_ARG(3));
  SET_NIL_RETURN();
}

static void
c__start(mrbc_vm *vm, mrbc_value *v, int argc)
{
  PWM_set_enabled(GET_INT_ARG(1), true);
  SET_INT_RETURN(0);
}

static void
c__stop(mrbc_vm *vm, mrbc_value *v, int argc)
{
  PWM_set_enabled(GET_INT_ARG(1), false);
  SET_INT_RETURN(0);
}

void
mrbc_pwm_init(void)
{
  mrbc_class *mrbc_class_PWM = mrbc_define_class(0, "PWM", mrbc_class_object);
  mrbc_define_method(0, mrbc_class_PWM, "_init", c__init);
  mrbc_define_method(0, mrbc_class_PWM, "_set_frequency_and_duty", c__set_frequency_and_duty);
  mrbc_define_method(0, mrbc_class_PWM, "_start", c__start);
  mrbc_define_method(0, mrbc_class_PWM, "_stop", c__stop);
}

#include <mrubyc.h>

static void
c_pulse_counter__init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int ret = PulseCounter_init(
    (uint32_t)GET_INT_ARG(1),
    (uint32_t)GET_INT_ARG(2),
    (uint32_t)GET_INT_ARG(3),
    GET_TT_ARG(4) == MRBC_TT_TRUE
  );
  SET_INT_RETURN(ret);
}

static void
c_pulse_counter__count(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int32_t count;
  if (PulseCounter_get_count(GET_INT_ARG(1), &count) != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "PulseCounter: failed to get count");
    return;
  }
  SET_INT_RETURN(count);
}

static void
c_pulse_counter__clear(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SET_INT_RETURN(PulseCounter_clear(GET_INT_ARG(1)));
}

void
mrbc_pulse_counter_init(mrbc_vm *vm)
{
  mrbc_class *mrbc_class_PulseCounter = mrbc_define_class(vm, "PulseCounter", mrbc_class_object);
  mrbc_define_method(vm, mrbc_class_PulseCounter, "_init", c_pulse_counter__init);
  mrbc_define_method(vm, mrbc_class_PulseCounter, "_count", c_pulse_counter__count);
  mrbc_define_method(vm, mrbc_class_PulseCounter, "_clear", c_pulse_counter__clear);
}

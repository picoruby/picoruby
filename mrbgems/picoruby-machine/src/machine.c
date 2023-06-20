#include <stdbool.h>
#include <mrubyc.h>
#include "../include/machine.h"


static void
c_Machine_sleep(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (GET_TT_ARG(1) != MRBC_TT_FIXNUM) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong type of arguments");
    return;
  }
  uint32_t sec = GET_INT_ARG(1);
  if (30 * 24 * 60 * 60 <= sec) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "sleep length must be less than 30 days");
    return;
  } else if (sec < 2) {
    // Hangs if you attempt to sleep for 1 second.
    console_printf("cannot sleep less than 2 sec\n");
  } else {
    console_printf("going to sleep %d sec\n", sec);
    Machine_sleep(sec);
  }
  SET_INT_RETURN(sec);
}

static void
c_Machine_deep_sleep(mrbc_vm *vm, mrbc_value *v, int argc)
{
  console_printf("not implemented\n");
}

void
mrbc_machine_init(void)
{
  mrbc_class *mrbc_class_Machine = mrbc_define_class(0, "Machine", mrbc_class_object);

  mrbc_define_method(0, mrbc_class_Machine, "sleep", c_Machine_sleep);
  mrbc_define_method(0, mrbc_class_Machine, "deep_sleep", c_Machine_deep_sleep);
}


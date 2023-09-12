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
  if (24 * 60 * 60 <= sec) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "sleep length must be less than 24 hours (86400 sec)");
    return;
  } else if (sec < 2) {
    // Hangs if you attempt to sleep for 1 second.
    console_printf("Cannot sleep less than 2 sec\n");
  } else {
    console_printf("Going to sleep %d sec (USC-CDC will not be back)\n", sec);
    Machine_sleep(sec);
  }
  SET_INT_RETURN(sec);
}

static void
c_Machine_deep_sleep(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "Not implemented");
  return;
  // TODO: Implement
  if (argc != 3) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (GET_TT_ARG(1) != MRBC_TT_FIXNUM) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong type of arguments");
    return;
  }
  console_printf("Going to deep sleep\n");
  Machine_deep_sleep(GET_INT_ARG(1), v[2].tt == MRBC_TT_TRUE, v[3].tt == MRBC_TT_TRUE);
  SET_INT_RETURN(0);
}

static void
c_Machine_watchdog_reboot(mrbc_vm *vm, mrbc_value *v, int argc)
{
  Machine_watchdog_reboot(GET_INT_ARG(1));
  SET_INT_RETURN(0);
}

static void
c_Machine_watchdog_caused_reboot_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (Machine_watchdog_caused_reboot()) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

void
mrbc_machine_init(void)
{
  mrbc_class *mrbc_class_Machine = mrbc_define_class(0, "Machine", mrbc_class_object);

  mrbc_define_method(0, mrbc_class_Machine, "sleep", c_Machine_sleep);
  mrbc_define_method(0, mrbc_class_Machine, "deep_sleep", c_Machine_deep_sleep);
  mrbc_define_method(0, mrbc_class_Machine, "watchdog_reboot", c_Machine_watchdog_reboot);
  mrbc_define_method(0, mrbc_class_Machine, "watchdog_caused_reboot?", c_Machine_watchdog_caused_reboot_q);
}


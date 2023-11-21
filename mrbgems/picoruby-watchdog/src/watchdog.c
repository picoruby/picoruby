#include <stdbool.h>
#include <mrubyc.h>
#include "../include/watchdog.h"

static void
c_Watchdog_reboot(mrbc_vm *vm, mrbc_value *v, int argc)
{
  Watchdog_reboot(GET_INT_ARG(1));
  SET_INT_RETURN(0);
}

static void
c_Watchdog_caused_reboot_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (Watchdog_caused_reboot()) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

void
mrbc_machine_init(void)
{
  mrbc_class *mrbc_class_Machine = mrbc_define_class(0, "Watchdog", mrbc_class_object);

  mrbc_define_method(0, mrbc_class_Machine, "reboot", c_Watchdog_reboot);
  mrbc_define_method(0, mrbc_class_Machine, "caused_reboot?", c_Watchdog_caused_reboot_q);
}


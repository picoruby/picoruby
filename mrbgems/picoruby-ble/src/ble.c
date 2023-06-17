#include <stdbool.h>
#include <mrubyc.h>
#include "../include/ble.h"


static void
c_init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_init();
  SET_INT_RETURN(0);
}

void
mrbc_ble_init(void)
{
  mrbc_class *mrbc_class_BLE = mrbc_define_class(0, "BLE", mrbc_class_object);

  mrbc_define_method(0, mrbc_class_BLE, "init", c_init);
}

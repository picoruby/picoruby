#include <stdbool.h>
#include <mrubyc.h>
#include "../include/ble.h"
#include "../include/ble_central.h"

static void
c_init(mrbc_vm *vm, mrbc_value *v, int argc)
{
}

void
mrbc_init_class_BLE_Central(void)
{
  mrbc_class *mrbc_class_BLE = mrbc_define_class(0, "BLE", mrbc_class_object);
  mrbc_value *BLE = mrbc_get_class_const(mrbc_class_BLE, mrbc_search_symid("Central"));
  mrbc_class *mrbc_class_BLE_Central = BLE->cls;

  mrbc_define_method(0, mrbc_class_BLE_Central, "init", c_init);
}

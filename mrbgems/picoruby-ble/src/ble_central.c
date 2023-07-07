#include <stdbool.h>
#include <mrubyc.h>
#include "../include/ble.h"
#include "../include/ble_central.h"


static void
c__init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (BLE_central_init() < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "BLE::Central init failed");
    return;
  }
  singleton.instance = v[0].instance;
}

static void
c_start_scan(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_central_start_scan();
  SET_INT_RETURN(0);
}

void
mrbc_init_class_BLE_Central(void)
{
  mrbc_class *mrbc_class_BLE = mrbc_define_class(0, "BLE", mrbc_class_object);
  mrbc_value *BLE = mrbc_get_class_const(mrbc_class_BLE, mrbc_search_symid("Central"));
  mrbc_class *mrbc_class_BLE_Central = BLE->cls;

  mrbc_define_method(0, mrbc_class_BLE_Central, "_init", c__init);
  mrbc_define_method(0, mrbc_class_BLE_Central, "start_scan", c_start_scan);
}

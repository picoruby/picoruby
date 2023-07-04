#include <stdbool.h>
#include <mrubyc.h>
#include "../include/ble.h"

static void
c_hci_power_on(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_hci_power_on();
}

void
mrbc_ble_init(void)
{
  mrbc_class *mrbc_class_BLE = mrbc_define_class(0, "BLE", mrbc_class_object);
  mrbc_define_method(0, mrbc_class_BLE, "hci_power_on", c_hci_power_on);

  mrbc_init_class_BLE_Peripheral();
  mrbc_init_class_BLE_Central();
}

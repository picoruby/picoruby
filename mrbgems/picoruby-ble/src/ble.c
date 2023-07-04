#include <stdbool.h>
#include <mrubyc.h>
#include "../include/ble.h"

void
mrbc_ble_init(void)
{
  mrbc_init_class_BLE_Peripheral();
  mrbc_init_class_BLE_Central();
}

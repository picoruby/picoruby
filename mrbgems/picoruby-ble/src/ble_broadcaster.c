#include <stdbool.h>
#include <mrubyc.h>
#include "../include/ble.h"
#include "../include/ble_peripheral.h"

static void
c_advertise(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value adv_data = GET_ARG(1);
  if (adv_data.tt == MRBC_TT_NIL) {
    BLE_peripheral_stop_advertise();
  } else {
    BLE_peripheral_advertise(adv_data.string->data, adv_data.string->size, false);
  }
  SET_INT_RETURN(0);
}

void
mrbc_init_class_BLE_Broadcaster(void)
{
  mrbc_class *mrbc_class_BLE = mrbc_define_class(0, "BLE", mrbc_class_object);
  mrbc_value *Broadcaster = mrbc_get_class_const(mrbc_class_BLE, mrbc_search_symid("Broadcaster"));
  mrbc_class *mrbc_class_BLE_Broadcaster = Broadcaster->cls;

  mrbc_define_method(0, mrbc_class_BLE_Broadcaster, "advertise", c_advertise);
}


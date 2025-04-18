#include "mrubyc.h"

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
mrbc_init_class_BLE_Broadcaster(mrbc_vm *vm, mrbc_class *class_BLE)
{
  mrbc_define_method(vm, class_BLE, "broadcaster_advertise", c_advertise);
}

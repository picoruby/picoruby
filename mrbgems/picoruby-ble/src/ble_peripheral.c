#include <stdbool.h>
#include <mrubyc.h>
#include "../include/ble.h"
#include "../include/ble_peripheral.h"

static void
c_advertise(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value adv_data = GET_ARG(1);
  BLE_peripheral_advertise(adv_data.string->data, adv_data.string->size);
}

static void
c_notify(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  BLE_peripheral_notify((uint16_t)GET_INT_ARG(1));
}

static void
c_request_can_send_now_event(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_peripheral_request_can_send_now_event();
}

static void
c_cyw43_arch_gpio_put(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int pin = GET_INT_ARG(1);
  int value;
  if (GET_TT_ARG(2) == MRBC_TT_TRUE) {
    value = 1;
  } else {
    value = 0;
  }
  BLE_peripheral_cyw43_arch_gpio_put(pin, value);
}

void
mrbc_init_class_BLE_Peripheral(void)
{
  mrbc_class *mrbc_class_BLE = mrbc_define_class(0, "BLE", mrbc_class_object);
  mrbc_value *BLE = mrbc_get_class_const(mrbc_class_BLE, mrbc_search_symid("Peripheral"));
  mrbc_class *mrbc_class_BLE_Peripheral = BLE->cls;

  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "advertise", c_advertise);
  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "notify", c_notify);

  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "request_can_send_now_event", c_request_can_send_now_event);
}


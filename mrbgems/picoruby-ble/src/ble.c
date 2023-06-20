#include <stdbool.h>
#include <mrubyc.h>
#include "../include/ble.h"

//typedef struct {
//  bool le_notification_enabled;
//  hci_con_handle_t con_handle;
//} picruby_ble_data;

static void
c_init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (BLE_init() < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "BLE init failed");
    return;
  }
  //mrbc_value ble = mrbc_instance_new(vm, v->cls, sizeof(picruby_ble_data));
  //ble->data.le_notification_enabled = false;
  //ble->data.con_handle = 0;
  //SET_RETURN(ble);
}

static void
c_hci_power_on(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_hci_power_on();
}

static void
c_packet_event(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint8_t event = BLE_packet_event();
  if (event == 0) {
    SET_NIL_RETURN();
  } else {
    SET_INT_RETURN(event);
  }
}

static void
c_down_packet_flag(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_down_packet_flag();
}

static void
c_advertise(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value adv_data = GET_ARG(1);
  BLE_advertise(adv_data.string->data, adv_data.string->size);
}

static void
c_enable_le_notification(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_enable_le_notification();
}

static void
c_notify(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_notify();
}

static void
c_gap_local_bd_addr(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint8_t addr[6];
  BLE_gap_local_bd_addr(addr);
  mrbc_value str = mrbc_string_new(vm, (const void *)addr, 6);
  SET_RETURN(str);
}

static void
c_heartbeat_on_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (BLE_heartbeat_on_q()) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_heartbeat_off(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_heartbeat_off();
}

static void
c_le_notification_enabled_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (BLE_le_notification_enabled_q()) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_request_can_send_now_event(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_request_can_send_now_event();
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
  BLE_cyw43_arch_gpio_put(pin, value);
}

static void
c_poll_temp(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_poll_temp();
}

void
mrbc_ble_init(void)
{
  mrbc_class *mrbc_class_BLE = mrbc_define_class(0, "BLE", mrbc_class_object);
  mrbc_value *v = mrbc_get_class_const(mrbc_class_BLE, mrbc_search_symid("AttServer"));
  mrbc_class *mrbc_class_BLE_AttServer = v->cls;

  mrbc_define_method(0, mrbc_class_BLE_AttServer, "init", c_init);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "hci_power_on", c_hci_power_on);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "packet_event", c_packet_event);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "down_packet_flag", c_down_packet_flag);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "advertise", c_advertise);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "enable_le_notification", c_enable_le_notification);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "notify", c_notify);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "gap_local_bd_addr", c_gap_local_bd_addr);

  mrbc_define_method(0, mrbc_class_BLE_AttServer, "heartbeat_on?", c_heartbeat_on_q);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "heartbeat_off", c_heartbeat_off);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "le_notification_enabled?", c_le_notification_enabled_q);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "request_can_send_now_event", c_request_can_send_now_event);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "cyw43_arch_gpio_put", c_cyw43_arch_gpio_put);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "poll_temp", c_poll_temp);
}

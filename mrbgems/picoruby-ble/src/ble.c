#include <stdbool.h>
#include <mrubyc.h>
#include "../include/ble.h"

//typedef struct {
//  bool le_notification_enabled;
//  hci_con_handle_t con_handle;
//} picruby_ble_data;

uint8_t packet_event_type = 0;
bool ble_heartbeat_on = false;
bool ble_notification_enabled = false;

static uint16_t current_temp = 0;
static uint8_t current_temp_str[2] = {0, 0};

uint16_t
PeripheralReadData(uint16_t att_handle, uint8_t **data)
{
  if (att_handle != 9) {
    return 0;
  }
  current_temp += 1;
  current_temp_str[0] = current_temp & 0xff;
  current_temp_str[1] = (current_temp >> 8) & 0xff;
  *data = current_temp_str;
  return sizeof(current_temp_str);
}

static void
c_init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (BLE_init(GET_STRING_ARG(1)) < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "BLE init failed");
    return;
  }
  /* Protect profile_data from GC */
  mrbc_incref(&v[1]);
}

static void
c_hci_power_on(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_hci_power_on();
}

static void
c_packet_event_type(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (packet_event_type == 0) {
    SET_NIL_RETURN();
  } else {
    SET_INT_RETURN(packet_event_type);
  }
}

static void
c_down_packet_flag(mrbc_vm *vm, mrbc_value *v, int argc)
{
  packet_event_type = 0;
}

static void
c_advertise(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value adv_data = GET_ARG(1);
  BLE_advertise(adv_data.string->data, adv_data.string->size);
}

static void
c_enable_notification(mrbc_vm *vm, mrbc_value *v, int argc)
{
  ble_notification_enabled = true;
}

static void
c_disable_notification(mrbc_vm *vm, mrbc_value *v, int argc)
{
  ble_notification_enabled = false;
}

static void
c_notify(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  BLE_notify((uint16_t)GET_INT_ARG(1));
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
  if (ble_heartbeat_on) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_heartbeat_off(mrbc_vm *vm, mrbc_value *v, int argc)
{
  ble_heartbeat_on = false;
}

static void
c_notification_enabled_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (ble_notification_enabled) {
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
c_heartbeat_period_ms_eq(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint16_t period_ms = (uint16_t)GET_INT_ARG(1);
  BLE_set_heartbeat_period_ms(period_ms);
  SET_INT_RETURN(period_ms);
}

void
mrbc_ble_init(void)
{
  mrbc_class *mrbc_class_BLE = mrbc_define_class(0, "BLE", mrbc_class_object);
  mrbc_value *v = mrbc_get_class_const(mrbc_class_BLE, mrbc_search_symid("Peripheral"));
  mrbc_class *mrbc_class_BLE_Peripheral = v->cls;

  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "init", c_init);
  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "heartbeat_period_ms=", c_heartbeat_period_ms_eq);
  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "hci_power_on", c_hci_power_on);
  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "packet_event_type", c_packet_event_type);
  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "down_packet_flag", c_down_packet_flag);
  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "advertise", c_advertise);
  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "enable_notification", c_enable_notification);
  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "disable_notification", c_disable_notification);
  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "notify", c_notify);
  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "gap_local_bd_addr", c_gap_local_bd_addr);

  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "heartbeat_on?", c_heartbeat_on_q);
  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "heartbeat_off", c_heartbeat_off);
  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "notification_enabled?", c_notification_enabled_q);
  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "request_can_send_now_event", c_request_can_send_now_event);
  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "cyw43_arch_gpio_put", c_cyw43_arch_gpio_put);
}


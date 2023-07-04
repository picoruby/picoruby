#include <stdbool.h>
#include <mrubyc.h>
#include "../include/ble_peripheral.h"

uint8_t packet_event_type = 0;
bool ble_heartbeat_on = false;

static mrbc_value peripheral = {0};

int
PeripheralWriteData(uint16_t att_handle, const uint8_t *data, uint16_t size)
{
  if (att_handle == 0 || size == 0 || peripheral.instance == NULL) return -1;
  mrbc_value write_values_hash = mrbc_instance_getiv(&peripheral, mrbc_str_to_symid("_write_values"));
  if (write_values_hash.tt != MRBC_TT_HASH) return -1;
  mrbc_value write_value = mrbc_string_new(NULL, data, size);
  mrbc_incref(&write_value);
  return mrbc_hash_set(&write_values_hash, &mrbc_integer_value(att_handle), &write_value);
}

int
PeripheralReadData(BLE_read_value *read_value)
{
  if (peripheral.instance == NULL) return -1;
  mrbc_value read_values_hash = mrbc_instance_getiv(&peripheral, mrbc_str_to_symid("_read_values"));
  if (read_values_hash.tt != MRBC_TT_HASH) return -1;
  mrbc_value value = mrbc_hash_get(&read_values_hash, &mrbc_integer_value(read_value->att_handle));
  if (value.tt != MRBC_TT_STRING) return -1;
  read_value->data = value.string->data;
  read_value->size = value.string->size;
  return 0;
}

static void
c_init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (BLE_init(GET_STRING_ARG(1)) < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "BLE init failed");
    return;
  }
  peripheral.instance = v[0].instance;
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
  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "notify", c_notify);
  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "gap_local_bd_addr", c_gap_local_bd_addr);

  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "heartbeat_on?", c_heartbeat_on_q);
  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "heartbeat_off", c_heartbeat_off);
  mrbc_define_method(0, mrbc_class_BLE_Peripheral, "request_can_send_now_event", c_request_can_send_now_event);
}


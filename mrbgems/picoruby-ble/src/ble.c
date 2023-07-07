#include <stdbool.h>
#include <mrubyc.h>
#include "../include/ble.h"

uint8_t packet_event_type = 0;
bool ble_heartbeat_on = false;

mrbc_value singleton = {0};

void
BLE_push_event(uint8_t *packet, uint16_t size)
{
  if (singleton.instance == NULL) return;
  mrbc_value _event_packets = mrbc_instance_getiv(&singleton, mrbc_str_to_symid("_event_packets"));
  if (_event_packets.tt != MRBC_TT_ARRAY) return;
  if (20 < _event_packets.array->n_stored) return;
  mrbc_value str = mrbc_string_new(NULL, (const void *)packet, size);
  mrbc_array_push(&_event_packets, &str);
}

static void
c_hci_power_on(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_hci_power_on();
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
c_heartbeat_period_ms_eq(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint16_t period_ms = (uint16_t)GET_INT_ARG(1);
  BLE_set_heartbeat_period_ms(period_ms);
  SET_INT_RETURN(period_ms);
}

static void
c_gap_local_bd_addr(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint8_t addr[6];
  BLE_gap_local_bd_addr(addr);
  mrbc_value str = mrbc_string_new(vm, (const void *)addr, 6);
  SET_RETURN(str);
}

void
mrbc_ble_init(void)
{
  mrbc_class *mrbc_class_BLE = mrbc_define_class(0, "BLE", mrbc_class_object);
  mrbc_define_method(0, mrbc_class_BLE, "hci_power_on", c_hci_power_on);
  mrbc_define_method(0, mrbc_class_BLE, "heartbeat_on?", c_heartbeat_on_q);
  mrbc_define_method(0, mrbc_class_BLE, "heartbeat_off", c_heartbeat_off);
  mrbc_define_method(0, mrbc_class_BLE, "heartbeat_period_ms=", c_heartbeat_period_ms_eq);
  mrbc_define_method(0, mrbc_class_BLE, "gap_local_bd_addr", c_gap_local_bd_addr);

  mrbc_init_class_BLE_Peripheral();
  mrbc_init_class_BLE_Central();
}

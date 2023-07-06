#include <stdbool.h>
#include <mrubyc.h>
#include "../include/ble.h"
#include "../include/ble_central.h"

static mrbc_value central = {0};

void
CentralPushEvent(uint8_t event_type, uint8_t *packet, uint16_t size)
{
  if (central.instance == NULL) return;
  mrbc_value _events = mrbc_instance_getiv(&central, mrbc_str_to_symid("_events"));
  if (_events.tt != MRBC_TT_ARRAY) return;
  if (_events.array->n_stored >= 10) return;
  mrbc_value element = mrbc_hash_new(NULL, 2);
  mrbc_hash_set(
    &element,
    &mrbc_symbol_value(mrbc_str_to_symid("event_type")),
    &mrbc_integer_value(event_type)
  );
  mrbc_value str = mrbc_string_new(NULL, (const void *)packet, size);
  mrbc_hash_set(
    &element,
    &mrbc_symbol_value(mrbc_str_to_symid("packet")),
    &str
  );
  mrbc_array_push(&_events, &element);
}

static void
c__init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (BLE_central_init() < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "BLE::Central init failed");
    return;
  }
  central.instance = v[0].instance;
}

static void
c_packet_event_state(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SET_INT_RETURN(BLE_central_packet_event_state());
}

static void
c_start_scan(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_central_start_scan();
  SET_INT_RETURN(0);
}

static void
c_get_packet(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint8_t *packet;
  uint16_t size = BLE_central_get_packet(packet);
  mrbc_value ret = mrbc_string_new(vm, (const void *)packet, size);
  SET_RETURN(ret);
}

void
mrbc_init_class_BLE_Central(void)
{
  mrbc_class *mrbc_class_BLE = mrbc_define_class(0, "BLE", mrbc_class_object);
  mrbc_value *BLE = mrbc_get_class_const(mrbc_class_BLE, mrbc_search_symid("Central"));
  mrbc_class *mrbc_class_BLE_Central = BLE->cls;

  mrbc_define_method(0, mrbc_class_BLE_Central, "_init", c__init);
  mrbc_define_method(0, mrbc_class_BLE_Central, "packet_event_state", c_packet_event_state);
  mrbc_define_method(0, mrbc_class_BLE_Central, "start_scan", c_start_scan);

  mrbc_define_method(0, mrbc_class_BLE_Central, "get_packet", c_get_packet);
}

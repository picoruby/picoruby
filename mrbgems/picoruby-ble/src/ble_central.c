#include <stdbool.h>
#include <mrubyc.h>
#include "../include/ble.h"
#include "../include/ble_central.h"

void
BLE_central_push_service(uint16_t start_group_handle, uint16_t end_group_handle, uint16_t uuid16, const uint8_t *uuid128)
{
  if (singleton.instance == NULL) return;
  mrbc_value _services = mrbc_instance_getiv(&singleton, mrbc_str_to_symid("_services"));
  if (_services.tt != MRBC_TT_ARRAY) return;
  mrbc_value hash = mrbc_hash_new(NULL, 4);
  mrbc_hash_set(&hash, &mrbc_symbol_value(mrbc_str_to_symid("start_group_handle")), &mrbc_integer_value(start_group_handle));
  mrbc_hash_set(&hash, &mrbc_symbol_value(mrbc_str_to_symid("end_group_handle")), &mrbc_integer_value(end_group_handle));
  mrbc_hash_set(&hash, &mrbc_symbol_value(mrbc_str_to_symid("uuid16")), &mrbc_integer_value(uuid16));
  mrbc_value uuid128_value = mrbc_string_new(NULL, uuid128, 16);
  mrbc_hash_set(&hash, &mrbc_symbol_value(mrbc_str_to_symid("uuid128")), &uuid128_value);
  mrbc_array_push(&_services, &hash);
}

static void
c_discover_primary_services(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (GET_TT_ARG(1) != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of arguments");
    return;
  }
  uint8_t res = BLE_discover_primary_services((uint16_t)GET_INT_ARG(1));
  SET_INT_RETURN(res);
}

static void
c_discover_characteristics_for_service(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (GET_TT_ARG(1) != MRBC_TT_INTEGER || GET_TT_ARG(2) != MRBC_TT_HASH) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of arguments");
    return;
  }
  mrbc_value service_hash = GET_ARG(2);
  uint8_t res = BLE_discover_characteristics_for_service(
    (uint16_t)GET_INT_ARG(1),
    mrbc_hash_get(&service_hash, &mrbc_symbol_value(mrbc_str_to_symid("start_group_handle"))).i,
    mrbc_hash_get(&service_hash, &mrbc_symbol_value(mrbc_str_to_symid("end_group_handle"))).i
  );
  SET_INT_RETURN(res);
}

static void
c_read_value_of_characteristic_using_value_handle(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (GET_TT_ARG(1) != MRBC_TT_INTEGER || GET_TT_ARG(2) != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of arguments");
    return;
  }
  uint8_t res = BLE_read_value_of_characteristic_using_value_handle(
    (uint16_t)GET_INT_ARG(1),
    (uint16_t)GET_INT_ARG(2)
  );
  SET_INT_RETURN(res);
}

static void
c_discover_characteristic_descriptors(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 3) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (GET_TT_ARG(1) != MRBC_TT_INTEGER || GET_TT_ARG(2) != MRBC_TT_INTEGER || GET_TT_ARG(3) != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of arguments");
    return;
  }
  uint8_t res = BLE_discover_characteristic_descriptors(
    (uint16_t)GET_INT_ARG(1),
    (uint16_t)GET_INT_ARG(2),
    (uint16_t)GET_INT_ARG(3)
  );
  SET_INT_RETURN(res);
}

static void
c_start_scan(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_central_start_scan();
  SET_INT_RETURN(0);
}

static void
c_stop_scan(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_central_stop_scan();
  SET_INT_RETURN(0);
}

static void
c_gap_connect(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (GET_TT_ARG(1) != MRBC_TT_STRING || GET_TT_ARG(2) != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of arguments");
    return;
  }
  uint8_t res = BLE_central_gap_connect(GET_STRING_ARG(1), GET_INT_ARG(2));
  SET_INT_RETURN(res);
}

void
mrbc_init_class_BLE_Central(void)
{
  mrbc_class *mrbc_class_BLE = mrbc_define_class(0, "BLE", mrbc_class_object);
  mrbc_value *BLE = mrbc_get_class_const(mrbc_class_BLE, mrbc_search_symid("Central"));
  mrbc_class *mrbc_class_BLE_Central = BLE->cls;

  mrbc_define_method(0, mrbc_class_BLE_Central, "start_scan", c_start_scan);
  mrbc_define_method(0, mrbc_class_BLE_Central, "stop_scan", c_stop_scan);
  mrbc_define_method(0, mrbc_class_BLE_Central, "gap_connect", c_gap_connect);
  mrbc_define_method(0, mrbc_class_BLE_Central, "discover_primary_services", c_discover_primary_services);
  mrbc_define_method(0, mrbc_class_BLE_Central, "discover_characteristics_for_service", c_discover_characteristics_for_service);
  mrbc_define_method(0, mrbc_class_BLE_Central, "read_value_of_characteristic_using_value_handle", c_read_value_of_characteristic_using_value_handle);
  mrbc_define_method(0, mrbc_class_BLE_Central, "discover_characteristic_descriptors", c_discover_characteristic_descriptors);
}

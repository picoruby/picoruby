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
  if (argc != 3) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (GET_TT_ARG(1) != MRBC_TT_INTEGER || GET_TT_ARG(2) != MRBC_TT_INTEGER || GET_TT_ARG(3) != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of arguments");
    return;
  }
  uint8_t res = BLE_discover_characteristics_for_service(
    (uint16_t)GET_INT_ARG(1),
    (uint16_t)GET_INT_ARG(2),
    (uint16_t)GET_INT_ARG(3)
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

/*
 * @param [Symbol] scan_type. :passive or :active
 * @param [Integer] scan_interval. 4..4000 (unit: 0.625ms)
 * @param [Integer] scan_window. 4..4000 (unit: 0.625ms)
 * @param [Symbol] scan_filter_policy (not implemented)
 * @return void
 */
static void
c_set_scan_params(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 3) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (GET_TT_ARG(1) != MRBC_TT_SYMBOL ||
      GET_TT_ARG(2) != MRBC_TT_INTEGER ||
      GET_TT_ARG(3) != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of arguments");
    return;
  }
  uint8_t scan_type;
  if (mrbc_str_to_symid("passive") == v[1].i) {
    scan_type = 0;
  } else if (mrbc_str_to_symid("active") == v[1].i) {
    scan_type = 1;
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "scan_type active is not implemented");
    return;
  } else {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong parameter of scan_type");
    return;
  }
  uint8_t scanning_filter_policy = 0; // TODO 1: all from whitelist
  BLE_central_set_scan_params(scan_type, (uint16_t)GET_INT_ARG(2), (uint16_t)GET_INT_ARG(3), scanning_filter_policy);
  SET_INT_RETURN(0);
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
mrbc_init_class_BLE_Central(mrbc_vm *vm, mrbc_class *class_BLE)
{
  mrbc_define_method(vm, class_BLE, "set_scan_params", c_set_scan_params);
  mrbc_define_method(vm, class_BLE, "start_scan", c_start_scan);
  mrbc_define_method(vm, class_BLE, "stop_scan", c_stop_scan);
  mrbc_define_method(vm, class_BLE, "gap_connect", c_gap_connect);
  mrbc_define_method(vm, class_BLE, "discover_primary_services", c_discover_primary_services);
  mrbc_define_method(vm, class_BLE, "discover_characteristics_for_service", c_discover_characteristics_for_service);
  mrbc_define_method(vm, class_BLE, "read_value_of_characteristic_using_value_handle", c_read_value_of_characteristic_using_value_handle);
  mrbc_define_method(vm, class_BLE, "discover_characteristic_descriptors", c_discover_characteristic_descriptors);
}

#include <stdbool.h>
#include <mrubyc.h>
#include "../include/ble.h"

uint8_t packet_event_type = 0;

mrbc_value singleton = {0};

static bool mutex_locked = false;

void
BLE_push_event(uint8_t *packet, uint16_t size)
{
  if (mutex_locked) return;
  if (singleton.instance == NULL) return;
  mrbc_value _event_packets = mrbc_instance_getiv(&singleton, mrbc_str_to_symid("_event_packets"));
  if (_event_packets.tt != MRBC_TT_ARRAY) return;
  if (20 < _event_packets.array->n_stored) return;
  mrbc_value str = mrbc_string_new(NULL, (const void *)packet, size);
  mrbc_array_push(&_event_packets, &str);
}

int
BLE_write_data(uint16_t att_handle, const uint8_t *data, uint16_t size)
{
  if (mutex_locked) return -1;
  if (att_handle == 0 || size == 0 || singleton.instance == NULL) return -1;
  mrbc_value write_values_hash = mrbc_instance_getiv(&singleton, mrbc_str_to_symid("_write_values"));
  if (write_values_hash.tt != MRBC_TT_HASH) return -1;
  mrbc_value write_value = mrbc_string_new(NULL, data, size);
  return mrbc_hash_set(&write_values_hash, &mrbc_integer_value(att_handle), &write_value);
}

int
BLE_read_data(BLE_read_value_t *read_value)
{
  if (singleton.instance == NULL) return -1;
  mrbc_value read_values_hash = mrbc_instance_getiv(&singleton, mrbc_str_to_symid("_read_values"));
  if (read_values_hash.tt != MRBC_TT_HASH) return -1;
  mrbc_value value = mrbc_hash_get(&read_values_hash, &mrbc_integer_value(read_value->att_handle));
  if (value.tt != MRBC_TT_STRING) return -1;
  read_value->data = value.string->data;
  read_value->size = value.string->size;
  return 0;
}

static void
c__init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  const uint8_t *profile_data;
  if (GET_TT_ARG(1) == MRBC_TT_STRING) {
    /* Protect profile_data from GC */
    mrbc_incref(&v[1]);
    profile_data = GET_STRING_ARG(1);
  } else if (GET_TT_ARG(1) == MRBC_TT_NIL) {
    profile_data = NULL;
  } else {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "BLE._init: wrong argument type");
    return;
  }
  int ble_role;
  int role_symid = mrbc_instance_getiv(&v[0], mrbc_str_to_symid("role")).i;
  if (role_symid == mrbc_str_to_symid("central")) {
    ble_role = BLE_ROLE_CENTRAL;
  } else if (role_symid == mrbc_str_to_symid("peripheral")) {
    ble_role = BLE_ROLE_PERIPHERAL;
  } else if (role_symid == mrbc_str_to_symid("observer")) {
    ble_role = BLE_ROLE_OBSERVER;
  } else if (role_symid == mrbc_str_to_symid("broadcaster")) {
    ble_role = BLE_ROLE_BROADCASTER;
  } else {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "BLE._init: wrong argument type");
    return;
  }
  if (BLE_init(profile_data, ble_role) < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "BLE init failed");
    return;
  }
  singleton.instance = v[0].instance;
}

static void
c_hci_power_control(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_hci_power_control(GET_INT_ARG(1));
  SET_INT_RETURN(0);
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
c_mutex_trylock(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (mutex_locked) {
    SET_FALSE_RETURN();
  } else {
    mutex_locked = true;
    SET_TRUE_RETURN();
  }
}

static void
c_mutex_unlock(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mutex_locked = false;
  SET_INT_RETURN(0);
}

void
mrbc_ble_init(void)
{
  mrbc_class *mrbc_class_BLE = mrbc_define_class(0, "BLE", mrbc_class_object);
  mrbc_define_method(0, mrbc_class_BLE, "_init", c__init);
  mrbc_define_method(0, mrbc_class_BLE, "hci_power_control", c_hci_power_control);
  mrbc_define_method(0, mrbc_class_BLE, "gap_local_bd_addr", c_gap_local_bd_addr);
  mrbc_define_method(0, mrbc_class_BLE, "mutex_trylock", c_mutex_trylock);
  mrbc_define_method(0, mrbc_class_BLE, "mutex_unlock", c_mutex_unlock);

  mrbc_init_class_BLE_Peripheral();
  mrbc_init_class_BLE_Central();
}

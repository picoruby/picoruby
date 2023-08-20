#include <stdbool.h>
#include <mrubyc.h>
#include "../include/ble.h"

mrbc_value singleton = {0};
static mrbc_value event_packets;
static volatile bool mutex_locked = false;

#define MAX_EVENT_PACKETS 10
inline void *
memmem(const void *l, size_t l_len, const void *s, size_t s_len)
{
  register char *cur, *last;
  const char *cl = (const char *)l;
  const char *cs = (const char *)s;

  if (l_len < s_len) return NULL;

  if (s_len == 1) return memchr(l, (int)*cs, l_len);

  last = (char *)cl + l_len - s_len;

  for (cur = (char *)cl; cur <= last; cur++)
    if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
      return cur;

  return NULL;
}

void
BLE_push_event(uint8_t *packet, uint16_t size)
{
  if (singleton.instance == NULL || packet == NULL) return;
  if (mutex_locked) return;
  mutex_locked = true;
  if (MAX_EVENT_PACKETS - 1 < event_packets.array->n_stored) {
    console_printf("[WARN] BLE_push_event: event packet dropped\n");
    console_printf("n_stored: %d\n", event_packets.array->n_stored);
  } else if (packet[0] == 0x60 || memmem((const void *)packet, size, "PicoRuby", 8)) {
    mrbc_value str = mrbc_string_new(NULL, (const void *)packet, size);
    mrbc_array_push(&event_packets, &str);
  } else {
    for (int i = 0; i < size; i++) {
      console_printf("%c", packet[i]);
    }
    console_printf("\n");
  }
  mutex_locked = false;
}

int
BLE_write_data(uint16_t att_handle, const uint8_t *data, uint16_t size)
{
  if (mutex_locked) return -1;
  mutex_locked = true;
  if (att_handle == 0 || size == 0 || singleton.instance == NULL) {
    mutex_locked = false;
    return -1;
  }
  mrbc_value write_values_hash = mrbc_instance_getiv(&singleton, mrbc_str_to_symid("_write_values"));
  if (write_values_hash.tt != MRBC_TT_HASH) {
    mutex_locked = false;
    return -1;
  }
  mrbc_value write_value = mrbc_string_new(NULL, data, size);
  mutex_locked = false;
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
  singleton.instance = v[0].instance;
  event_packets = mrbc_instance_getiv(&singleton, mrbc_str_to_symid("_event_packets"));
  mutex_locked = false;
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
    mrbc_raise(vm, MRBC_CLASS(TypeError), "BLE._init: wrong role type");
    return;
  }
  if (BLE_init(profile_data, ble_role) < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "BLE init failed");
    return;
  }
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
  mrbc_init_class_BLE_Broadcaster();
  mrbc_init_class_BLE_Central();
}

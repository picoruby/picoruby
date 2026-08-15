#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/string.h"
#include "mruby/hash.h"
#include "mruby/array.h"
#include "task.h"

/*
 * GC strategy: only the current BLE instance is pinned with
 * mrb_gc_register (BTstack is a hardware singleton, so there is at
 * most one). The event queue and the value hashes are instance
 * variables of that instance, so they stay alive through it and get
 * collected together with it when the next _init unpins it. The
 * statics below are mere caches for the BTstack callbacks; they are
 * always backed by the pinned instance, so they cannot dangle.
 */
static mrb_state *_mrb = NULL;
static mrb_value registered_ble;
static bool ble_registered = false;
static mrb_value write_values;
static mrb_value read_values;
static mrb_value event_queue;
/* C-heap copy of the GATT profile. BTstack keeps a raw pointer to it
 * indefinitely, which is why it is not owned by a Ruby object. */
static uint8_t *profile_buf = NULL;
static uint8_t pending_event_count;

#define BLE_MAX_PENDING_EVENTS 16

void
BLE_push_event(uint8_t *data, uint16_t size)
{
  if (_mrb == NULL || mrb_nil_p(event_queue) ||
      BLE_MAX_PENDING_EVENTS <= pending_event_count) return;
  mrb_value event = mrb_str_new(_mrb, (const char *)data, size);
  if (mrb_task_queue_push(_mrb, event_queue, event) == MRB_TASK_QUEUE_PUSH_OK) {
    pending_event_count++;
  }
}

void
BLE_heartbeat(void)
{
  if (_mrb == NULL || mrb_nil_p(event_queue) ||
      BLE_MAX_PENDING_EVENTS <= pending_event_count) return;
  mrb_state *mrb = _mrb;
  if (mrb_task_queue_push(mrb, event_queue, mrb_symbol_value(MRB_SYM(heartbeat))) ==
      MRB_TASK_QUEUE_PUSH_OK) {
    pending_event_count++;
  }
}

static mrb_value
mrb_event_popped(mrb_state *mrb, mrb_value self)
{
  if (0 < pending_event_count) pending_event_count--;
  return mrb_nil_value();
}

static mrb_value
mrb_event_queue_cleared(mrb_state *mrb, mrb_value self)
{
  pending_event_count = 0;
  return mrb_nil_value();
}

int
BLE_write_data(uint16_t att_handle, const uint8_t *data, uint16_t size)
{
  if (att_handle == 0 || size == 0 || _mrb == NULL || mrb_hash_p(write_values) == false) {
    return -1;
  }
  mrb_value key = mrb_fixnum_value(att_handle);
  mrb_value write_value = mrb_str_new(_mrb, (const char *)data, size);
  write_values_mutex = true;
  mrb_value queue = mrb_hash_get(_mrb, write_values, key);
  if (!mrb_array_p(queue)) {
    queue = mrb_ary_new_capa(_mrb, 4);
    mrb_hash_set(_mrb, write_values, key, queue);
  }
  mrb_ary_push(_mrb, queue, write_value);
  write_values_mutex = false;
  return 0;
}

int
BLE_read_data(BLE_read_value_t *read_value)
{
  if (_mrb == NULL || mrb_hash_p(read_values) == false) return -1;
  mrb_value value = mrb_hash_get(_mrb, read_values, mrb_fixnum_value(read_value->att_handle));
  if (mrb_string_p(value) == false) return -1;
  read_value->data = (uint8_t *)RSTRING_PTR(value);
  read_value->size = (uint16_t)RSTRING_LEN(value);
  return 0;
}


static mrb_value
mrb_pop_write_value(mrb_state *mrb, mrb_value self)
{
  if (write_values_mutex) return mrb_nil_value();
  mrb_int handle;
  mrb_get_args(mrb, "i", &handle);
  mrb_value key = mrb_fixnum_value(handle);
  mrb_value queue = mrb_hash_get(mrb, write_values, key);
  if (!mrb_array_p(queue) || RARRAY_LEN(queue) == 0) {
    return mrb_nil_value();
  }
  return mrb_ary_shift(mrb, queue);
}

static mrb_value
mrb_push_read_value(mrb_state *mrb, mrb_value self)
{
  mrb_int handle;
  mrb_value read_value;
  mrb_get_args(mrb, "iS", &handle, &read_value);
  mrb_hash_set(mrb, read_values, mrb_fixnum_value(handle), read_value);
  return read_value;
}

static mrb_value
mrb__init(mrb_state *mrb, mrb_value self)
{
  mrb_value profile;
  mrb_get_args(mrb, "o", &profile);

  if (!mrb_string_p(profile) && !mrb_nil_p(profile)) {
    mrb_raise(mrb, E_TYPE_ERROR, "BLE._init: wrong argument type");
  }
  int ble_role;
  mrb_value role = mrb_iv_get(mrb, self, MRB_IVSYM(role));
  switch (mrb_symbol(role)) {
    case MRB_SYM(central):
      ble_role = BLE_ROLE_CENTRAL;
      break;
    case MRB_SYM(peripheral):
      ble_role = BLE_ROLE_PERIPHERAL;
      break;
    case MRB_SYM(observer):
      ble_role = BLE_ROLE_OBSERVER;
      break;
    case MRB_SYM(broadcaster):
      ble_role = BLE_ROLE_BROADCASTER;
      break;
    default:
      mrb_raise(mrb, E_TYPE_ERROR, "BLE._init: wrong role type");
  }

  uint8_t *new_profile = NULL;
  size_t new_profile_len = 0;
  if (ble_role == BLE_ROLE_PERIPHERAL && mrb_string_p(profile)) {
    new_profile_len = (size_t)RSTRING_LEN(profile);
    new_profile = (uint8_t *)mrb_malloc(mrb, new_profile_len);
    memcpy(new_profile, RSTRING_PTR(profile), new_profile_len);
  }
  if (BLE_init(new_profile, ble_role) < 0) {
    if (new_profile) mrb_free(mrb, new_profile);
    mrb_raise(mrb, E_RUNTIME_ERROR, "BLE init failed");
  }
  /* BLE_init() replaces the ATT database for every role (possibly
   * with no database), so the previous profile is unreferenced now
   * and must be released even when the new role has none. */
  if (profile_buf) mrb_free(mrb, profile_buf);
  profile_buf = new_profile;

  /* The value hashes are created in ble.rb as instance variables so
   * the GC keeps them alive through the pinned instance. */
  mrb_value new_write_values = mrb_iv_get(mrb, self, MRB_IVSYM(write_values));
  mrb_value new_read_values = mrb_iv_get(mrb, self, MRB_IVSYM(read_values));
  if (!mrb_hash_p(new_write_values) || !mrb_hash_p(new_read_values)) {
    mrb_raise(mrb, E_TYPE_ERROR, "BLE._init: value hashes not initialized");
  }

  /* Pin the new instance before unpinning the previous one so the
   * statics never point into an unpinned object graph. The identity
   * check keeps the pin count at exactly one even if _init runs
   * twice on the same instance: registering again would stack a
   * duplicate, and mrb_gc_unregister removes ALL occurrences, so
   * unregistering self would undo the pin instead. */
  mrb_value prev_ble = registered_ble;
  bool same_instance = ble_registered && mrb_obj_eq(mrb, prev_ble, self);
  bool release_prev = ble_registered && !same_instance;
  if (!same_instance) {
    mrb_gc_register(mrb, self);
  }
  registered_ble = self;
  ble_registered = true;

  _mrb = mrb;
  event_queue = mrb_iv_get(mrb, self, MRB_IVSYM(event_queue));
  write_values = new_write_values;
  read_values = new_read_values;
  pending_event_count = 0;

  if (release_prev) {
    mrb_gc_unregister(mrb, prev_ble);
  }
  return self;
}

static mrb_value
mrb_hci_power_control(mrb_state *mrb, mrb_value self)
{
  mrb_int power_mode;
  mrb_get_args(mrb, "i", &power_mode);
  BLE_hci_power_control((uint8_t)power_mode);
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_gap_local_bd_addr(mrb_state *mrb, mrb_value self)
{
  uint8_t addr[6];
  BLE_gap_local_bd_addr(addr);
  return mrb_str_new(mrb, (const char *)addr, 6);
}

void
mrb_picoruby_ble_gem_init(mrb_state* mrb)
{
  struct RClass *class_BLE = mrb_define_class_id(mrb, MRB_SYM(BLE), mrb->object_class);

  mrb_define_private_method_id(mrb, class_BLE, MRB_SYM(_init), mrb__init, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_BLE, MRB_SYM(hci_power_control), mrb_hci_power_control, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_BLE, MRB_SYM(gap_local_bd_addr), mrb_gap_local_bd_addr, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_BLE, MRB_SYM(pop_write_value), mrb_pop_write_value, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_BLE, MRB_SYM(push_read_value), mrb_push_read_value, MRB_ARGS_REQ(2));
  mrb_define_private_method_id(mrb, class_BLE, MRB_SYM(_event_popped), mrb_event_popped, MRB_ARGS_NONE());
  mrb_define_private_method_id(mrb, class_BLE, MRB_SYM(_event_queue_cleared), mrb_event_queue_cleared, MRB_ARGS_NONE());
  mrb_init_class_BLE_Peripheral(mrb, class_BLE);
  mrb_init_class_BLE_Broadcaster(mrb, class_BLE);
  mrb_init_class_BLE_Central(mrb, class_BLE);
}

void
mrb_picoruby_ble_gem_final(mrb_state* mrb)
{
  if (profile_buf) {
    mrb_free(mrb, profile_buf);
    profile_buf = NULL;
  }
  ble_registered = false;
  _mrb = NULL;
}

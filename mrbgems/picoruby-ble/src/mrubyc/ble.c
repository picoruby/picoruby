static mrbc_value write_values = {.tt = MRBC_TT_NIL};
static mrbc_value read_values = {.tt = MRBC_TT_NIL};
static mrbc_value event_queue = {.tt = MRBC_TT_NIL};
/* C-heap copy of the GATT profile. BTstack keeps a raw pointer to it
 * indefinitely, which is why it is not owned by a Ruby object. */
static uint8_t *profile_buf = NULL;
static uint8_t pending_event_count;

#define BLE_MAX_PENDING_EVENTS 16

#define NODE_BOX_SIZE 10
#define VM_REGS_SIZE 110 // can be reduced?

void
BLE_push_event(uint8_t *data, uint16_t size)
{
  if (event_queue.tt == MRBC_TT_NIL ||
      BLE_MAX_PENDING_EVENTS <= pending_event_count) return;
  mrbc_value event = mrbc_string_new(NULL, (const void *)data, size);
  if (mrbc_task_queue_push(&event_queue, &event) == MRBC_TASK_QUEUE_PUSH_OK) {
    pending_event_count++;
  }
  mrbc_decref(&event);
}

void
BLE_heartbeat(void)
{
  if (event_queue.tt == MRBC_TT_NIL ||
      BLE_MAX_PENDING_EVENTS <= pending_event_count) return;
  mrbc_value event = mrbc_symbol_value(mrbc_str_to_symid("heartbeat"));
  if (mrbc_task_queue_push(&event_queue, &event) == MRBC_TASK_QUEUE_PUSH_OK) {
    pending_event_count++;
  }
}

static void
c_event_popped(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (0 < pending_event_count) pending_event_count--;
  SET_NIL_RETURN();
}

static void
c_event_queue_cleared(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pending_event_count = 0;
  SET_NIL_RETURN();
}

int
BLE_write_data(uint16_t att_handle, const uint8_t *data, uint16_t size)
{
  if (att_handle == 0 || size == 0 || write_values.tt == MRBC_TT_NIL) {
    return -1;
  }
  mrbc_value key = mrbc_integer_value(att_handle);
  mrbc_value write_value = mrbc_string_new(NULL, (const void *)data, size);
  write_values_mutex = true;
  mrbc_value queue = mrbc_hash_get(&write_values, &key);
  if (queue.tt != MRBC_TT_ARRAY) {
    queue = mrbc_array_new(NULL, 4);
    mrbc_hash_set(&write_values, &key, &queue);
  }
  mrbc_array_push(&queue, &write_value);
  write_values_mutex = false;
  return 0;
}

int
BLE_read_data(BLE_read_value_t *read_value)
{
  if (read_values.tt == MRBC_TT_NIL) {
    return -1;
  }
  mrbc_value value = mrbc_hash_get(&read_values, &mrbc_integer_value(read_value->att_handle));
  if (value.tt != MRBC_TT_STRING) return -1;
  read_value->data = value.string->data;
  read_value->size = value.string->size;
  return 0;
}


static void
c_pop_write_value(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (write_values_mutex) {
    SET_NIL_RETURN();
    return;
  }
  mrbc_value key = GET_ARG(1);
  mrbc_value queue = mrbc_hash_get(&write_values, &key);
  if (queue.tt != MRBC_TT_ARRAY || mrbc_array_size(&queue) == 0) {
    SET_NIL_RETURN();
    return;
  }
  mrbc_value value = mrbc_array_shift(&queue);
  SET_RETURN(value);
}

static void
c_push_read_value(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value handle= GET_ARG(1);
  mrbc_value read_value = GET_ARG(2);
  mrbc_incref(&read_value);  // for hash ownership
  mrbc_hash_set(&read_values, &handle, &read_value);
  mrbc_incref(&read_value);  // for return value
  SET_RETURN(read_value);
}

static void
c__init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (GET_TT_ARG(1) != MRBC_TT_STRING && GET_TT_ARG(1) != MRBC_TT_NIL) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "BLE._init: wrong argument type");
    return;
  }
  int ble_role;
  int role_symid = mrbc_instance_getiv(&v[0], mrbc_str_to_symid("role")).sym_id;
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

  uint8_t *new_profile = NULL;
  if (ble_role == BLE_ROLE_PERIPHERAL && GET_TT_ARG(1) == MRBC_TT_STRING) {
    /* Copy the profile to the C heap. BTstack keeps a raw pointer to
     * it, so a refcounted Ruby string is the wrong owner (the old
     * code leaked one copy per BLE.new). */
    int len = v[1].string->size;
    new_profile = mrbc_raw_alloc(len + 1);
    if (new_profile == NULL) {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "BLE._init: no memory");
      return;
    }
    memcpy(new_profile, v[1].string->data, len);
    new_profile[len] = 0;
  }
  Machine_tud_task();
  if (BLE_init(new_profile, ble_role) < 0) {
    if (new_profile) mrbc_raw_free(new_profile);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "BLE init failed");
    return;
  }
  /* BLE_init() replaces the ATT database for every role (possibly
   * with no database), so the previous profile is unreferenced now
   * and must be released even when the new role has none. */
  if (profile_buf) mrbc_raw_free(profile_buf);
  profile_buf = new_profile;

  /* Take references to the new instance's objects, then release the
   * previous instance's ones (mrbc_instance_getiv increfs). The
   * swap order keeps the statics valid at every point. */
  mrbc_value prev_event_queue = event_queue;
  mrbc_value prev_write_values = write_values;
  mrbc_value prev_read_values = read_values;
  event_queue = mrbc_instance_getiv(&v[0], mrbc_str_to_symid("event_queue"));
  write_values = mrbc_instance_getiv(&v[0], mrbc_str_to_symid("write_values"));
  read_values = mrbc_instance_getiv(&v[0], mrbc_str_to_symid("read_values"));
  pending_event_count = 0;
  mrbc_decref(&prev_event_queue);
  mrbc_decref(&prev_write_values);
  mrbc_decref(&prev_read_values);

  Machine_tud_task();
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

void
mrbc_ble_init(mrbc_vm *vm)
{
  mrbc_class *class_BLE = mrbc_define_class(vm, "BLE", mrbc_class_object);
  mrbc_define_method(vm, class_BLE, "_init", c__init);
  mrbc_define_method(vm, class_BLE, "hci_power_control", c_hci_power_control);
  mrbc_define_method(vm, class_BLE, "gap_local_bd_addr", c_gap_local_bd_addr);
  mrbc_define_method(vm, class_BLE, "pop_write_value", c_pop_write_value);
  mrbc_define_method(vm, class_BLE, "push_read_value", c_push_read_value);
  mrbc_define_method(vm, class_BLE, "_event_popped", c_event_popped);
  mrbc_define_method(vm, class_BLE, "_event_queue_cleared", c_event_queue_cleared);
  mrbc_init_class_BLE_Peripheral(vm, class_BLE);
  mrbc_init_class_BLE_Broadcaster(vm, class_BLE);
  mrbc_init_class_BLE_Central(vm, class_BLE);
}

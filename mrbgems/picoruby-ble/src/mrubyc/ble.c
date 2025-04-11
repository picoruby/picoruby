static mrbc_value write_values = {.tt = MRBC_TT_NIL};
static mrbc_value read_values = {.tt = MRBC_TT_NIL};

#define NODE_BOX_SIZE 10
#define VM_REGS_SIZE 110 // can be reduced?

void
BLE_push_event(uint8_t *data, uint16_t size)
{
  if (packet_mutex) return;
  packet_mutex = true;
  packet_flag = true;
  packet_size = size;
  if (packet != NULL) {
    mrbc_raw_free(packet);
  }
  packet = mrbc_raw_alloc(packet_size);
  memcpy(packet, data, packet_size);
  packet_mutex = false;
}

void
BLE_heartbeat(void)
{
  if (packet_mutex) return;
  heatbeat_flag = true;
}

int
BLE_write_data(uint16_t att_handle, const uint8_t *data, uint16_t size)
{
  if (att_handle == 0 || size == 0 || write_values.tt == MRBC_TT_NIL) {
    return -1;
  }
  write_values_mutex = true;
  mrbc_value write_value = mrbc_string_new(NULL, (const void *)data, size);
  write_values_mutex = false;
  return mrbc_hash_set(&write_values, &mrbc_integer_value(att_handle), &write_value);
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
c_pop_heartbeat(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (heatbeat_flag) {
    heatbeat_flag = false;
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_pop_packet(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (packet_mutex || !packet_flag) {
    SET_NIL_RETURN();
    return;
  }
  packet_mutex = true;
  packet_flag = false;
  mrb_value packet_value = mrbc_string_new(vm, (const char *)packet, packet_size);
  mrbc_raw_free(packet);
  packet = NULL;
  packet_mutex = false;
  SET_RETURN(packet_value);
}

static void
c_pop_write_value(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (write_values_mutex) {
    SET_NIL_RETURN();
    return;
  }
  mrb_value handle = GET_ARG(1);
  mrbc_value write_value = mrbc_hash_remove(&write_values, &handle);
  SET_RETURN(write_value);
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
  mrbc_hash_set(&read_values, &handle, &read_value);
  SET_RETURN(read_value);
}

static void
c__init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  write_values = mrbc_hash_new(vm, 0);
  read_values = mrbc_hash_new(vm, 0);

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
  Machine_tud_task();
  if (BLE_init(profile_data, ble_role) < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "BLE init failed");
    return;
  }
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
  mrbc_define_method(vm, class_BLE, "pop_heartbeat", c_pop_heartbeat);
  mrbc_define_method(vm, class_BLE, "pop_packet", c_pop_packet);

  mrbc_init_class_BLE_Peripheral(vm, class_BLE);
  mrbc_init_class_BLE_Broadcaster(vm, class_BLE);
  mrbc_init_class_BLE_Central(vm, class_BLE);
}


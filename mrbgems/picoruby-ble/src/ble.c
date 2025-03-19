#include <stdbool.h>
#include "picoruby.h"
#include "../include/ble.h"
#include "../include/ble_central.h"

#include "../../picoruby-machine/include/machine.h"

static mrbc_vm *packet_callback_vm = NULL;
static mrbc_vm *heartbeat_callback_vm = NULL;
mrbc_value singleton = {.tt = MRBC_TT_NIL};

#define NODE_BOX_SIZE 10
#define VM_REGS_SIZE 110 // can be reduced?

static mrbc_vm *
prepare_vm(mrbc_vm *vm, const char *script)
{
  Machine_tud_task();
  mrc_ccontext *cc = mrc_ccontext_new(NULL);
  Machine_tud_task();
  mrc_irep *irep = mrc_load_string_cxt(cc, (const uint8_t **)&script, strlen(script));
  uint8_t flags = 0;
  uint8_t *vm_code = NULL;
  size_t vm_code_size = 0;
  Machine_tud_task();
  int result = mrc_dump_irep(cc, irep, flags, &vm_code, &vm_code_size);
  if (result != MRC_DUMP_OK) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Failed to dump VM code");
    mrc_ccontext_free(cc);
    return NULL;
  }
  Machine_tud_task();
  mrbc_vm *new_vm = mrbc_vm_new(VM_REGS_SIZE);
  Machine_tud_task();
  result = mrbc_load_mrb(new_vm, vm_code);
  mrc_irep_free(cc, irep);
  mrc_ccontext_free(cc);
  if (result) {
    mrbc_print_exception(&new_vm->exception);
    return NULL;
  }
  mrbc_vm_begin(new_vm);
  Machine_tud_task();
  return new_vm;
}


/*
 * Workaround: To avoid deadlock
 */
static bool mutex_locked = false;

static void
reset_vm(mrbc_vm *vm)
{
  vm->cur_irep        = vm->top_irep;
  vm->inst            = vm->cur_irep->inst;
  vm->cur_regs        = vm->regs;
  vm->target_class    = mrbc_class_object;
  vm->callinfo_tail   = NULL;
  vm->ret_blk         = NULL;
  vm->exception       = mrbc_nil_value();
  vm->flag_preemption = 0;
  vm->flag_stop       = 0;
}

void
BLE_push_event(uint8_t *packet, uint16_t size)
{
  if (mutex_locked || packet_callback_vm == NULL || packet == NULL) return;
  mutex_locked = true;
  mrbc_sym sym_id = mrbc_str_to_symid("$_btstack_event_packet");
  mrbc_value *event_packet = mrbc_get_global(sym_id);
  if (event_packet->tt == MRBC_TT_STRING && memcmp(event_packet->string->data, packet, size) == 0) {
    // same as previous packet but not happens?
  } else {
    mrbc_value str = mrbc_string_new(packet_callback_vm, (const void *)packet, size);
    mrbc_set_global(sym_id, &str);
    mrbc_vm_run(packet_callback_vm);
    reset_vm(packet_callback_vm);
  }
  mutex_locked = false;
}

void
BLE_heartbeat(void)
{
  if (heartbeat_callback_vm == NULL || mutex_locked) return;
  mrbc_vm_run(heartbeat_callback_vm);
  reset_vm(heartbeat_callback_vm);
}

int
BLE_write_data(uint16_t att_handle, const uint8_t *data, uint16_t size)
{
  if (att_handle == 0 || size == 0 || singleton.instance == NULL) {
    return -1;
  }
  mrbc_value write_values_hash = mrbc_instance_getiv(&singleton, mrbc_str_to_symid("_write_values"));
  if (write_values_hash.tt != MRBC_TT_HASH) {
    return -1;
  }
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
  Machine_tud_task();
  if (packet_callback_vm == NULL) {
    packet_callback_vm = prepare_vm(vm, "BLE.instance&.packet_callback($_btstack_event_packet)");
  }
  Machine_tud_task();
  if (heartbeat_callback_vm == NULL) {
    heartbeat_callback_vm = prepare_vm(vm, "BLE.instance&.heartbeat_callback");
  }
  singleton.instance = v[0].instance;
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

  mrbc_init_class_BLE_Peripheral(vm, class_BLE);
  mrbc_init_class_BLE_Broadcaster(vm, class_BLE);
  mrbc_init_class_BLE_Central(vm, class_BLE);
}

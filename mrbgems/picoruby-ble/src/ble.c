#include <stdbool.h>
#include <mrubyc.h>
#include "../include/ble.h"

//typedef struct {
//  bool le_notification_enabled;
//  hci_con_handle_t con_handle;
//} picruby_ble_data;

static void
c_init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (BLE_init() < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "BLE init failed");
    return;
  }
  //mrbc_value ble = mrbc_instance_new(vm, v->cls, sizeof(picruby_ble_data));
  //ble->data.le_notification_enabled = false;
  //ble->data.con_handle = 0;
  //SET_RETURN(ble);
}

static void
c__start(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_start();
}

static void
c_packet_flag(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (BLE_packet_flag()) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_down_packet_flag(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_down_packet_flag();
}

void
mrbc_ble_init(void)
{
  mrbc_class *mrbc_class_BLE = mrbc_define_class(0, "BLE", mrbc_class_object);
  mrbc_value *v = mrbc_get_class_const(mrbc_class_BLE, mrbc_search_symid("AttServer"));
  mrbc_class *mrbc_class_BLE_AttServer = v->cls;

  mrbc_define_method(0, mrbc_class_BLE_AttServer, "init", c_init);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "_start", c__start);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "packet_flag?", c_packet_flag);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "down_packet_flag", c_down_packet_flag);
}

uint16_t
picoruby_att_read_callback(uint16_t conn_handle, uint16_t attr_handle, uint8_t *p_value, uint16_t len)
{
  if (attr_handle == 0x0003) {
    p_value[0] = 0x01;
    return 1;
  }
  return 0;
}

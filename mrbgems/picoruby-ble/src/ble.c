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
c_packet_event(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int event = BLE_packet_event();
  if (event < 0) {
    SET_NIL_RETURN();
  } else {
    SET_INT_RETURN(event);
  }
}

static void
c_down_packet_flag(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_down_packet_flag();
}

static void
c_advertise(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_advertise();
}

static void
c_enable_le_notification(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_enable_le_notification();
}

static void
c_notify(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_notify();
}

void
mrbc_ble_init(void)
{
  mrbc_class *mrbc_class_BLE = mrbc_define_class(0, "BLE", mrbc_class_object);
  mrbc_value *v = mrbc_get_class_const(mrbc_class_BLE, mrbc_search_symid("AttServer"));
  mrbc_class *mrbc_class_BLE_AttServer = v->cls;

  mrbc_define_method(0, mrbc_class_BLE_AttServer, "init", c_init);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "_start", c__start);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "packet_event", c_packet_event);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "down_packet_flag", c_down_packet_flag);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "advertise", c_advertise);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "enable_le_notification", c_enable_le_notification);
  mrbc_define_method(0, mrbc_class_BLE_AttServer, "notify", c_notify);
}


#include <stdbool.h>
#include <mrubyc.h>
#include "../include/ble.h"

typedef struct {
  bool le_notification_enabled;
  hci_con_handle_t con_handle;
} picruby_ble_data;

static void
c_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (BLE_init() < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "BLE init failed");
    return;
  }
  mrbc_value ble = mrbc_instance_new(vm, v->cls, sizeof(picruby_ble_data));
  ble->data.le_notification_enabled = false;
  ble->data.con_handle = 0;
  SET_RETURN(ble);
}

static void
c_start(mrbc_vm *vm, mrbc_value *v, int argc)
{
  BLE_start();
  SET_INT_RETURN(0);
}

void
mrbc_ble_init(void)
{
  mrbc_class *mrbc_class_BLE = mrbc_define_class(0, "BLE", mrbc_class_object);

  mrbc_define_method(0, mrbc_class_BLE, "new", c_new);
  mrbc_define_method(0, mrbc_class_BLE, "start", c_start);
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

#ifndef BLE_DEFINED_H_
#define BLE_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum BLE_role_t {
  BLE_ROLE_NONE = 0,
  BLE_ROLE_CENTRAL,
  BLE_ROLE_PERIPHERAL,
  BLE_ROLE_BROADCASTER,
  BLE_ROLE_OBSERVER
};

#if defined(PICORB_VM_MRUBY)
#include "mruby.h"
void mrb_init_class_BLE_Peripheral(mrb_state *mrb, struct RClass *class_BLE);
void mrb_init_class_BLE_Broadcaster(mrb_state *mrb, struct RClass *class_BLE);
void mrb_init_class_BLE_Central(mrb_state *mrb, struct RClass *class_BLE);
#elif defined(PICORB_VM_MRUBYC)
#include "mrubyc.h"
void mrbc_init_class_BLE_Peripheral(mrbc_vm *vm, mrbc_class *class_BLE);
void mrbc_init_class_BLE_Broadcaster(mrbc_vm *vm, mrbc_class *class_BLE);
void mrbc_init_class_BLE_Central(mrbc_vm *vm, mrbc_class *class_BLE);
#endif

typedef struct {
  uint16_t att_handle;
  uint8_t *data;
  uint16_t size;
} BLE_read_value_t;

int BLE_init(const uint8_t *profile, int ble_role);
void BLE_hci_power_control(uint8_t power_mode);
void BLE_gap_local_bd_addr(uint8_t *local_addr);
void BLE_push_event(uint8_t *packet, uint16_t size);
void BLE_heartbeat(void);
int BLE_write_data(uint16_t att_handle, const uint8_t *data, uint16_t size);
int BLE_read_data(BLE_read_value_t *read_value);
uint8_t BLE_discover_primary_services(uint16_t conn_handle);
uint8_t BLE_discover_characteristics_for_service(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle);
uint8_t BLE_read_value_of_characteristic_using_value_handle(uint16_t conn_handle, uint16_t value_handle);
uint8_t BLE_discover_characteristic_descriptors(uint16_t conn_handle, uint16_t value_handle, uint16_t end_handle);

#ifdef __cplusplus
}
#endif

#endif /* BLE_DEFINED_H_ */


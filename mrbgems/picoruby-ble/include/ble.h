#ifndef BLE_DEFINED_H_
#define BLE_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>
#include <mrubyc.h>

#ifdef __cplusplus
extern "C" {
#endif

extern mrbc_value singleton;

void mrbc_init_class_BLE_Peripheral(void);
void mrbc_init_class_BLE_Central(void);

typedef struct {
  uint16_t att_handle;
  uint8_t *data;
  uint16_t size;
} BLE_read_value_t;

int BLE_init(const uint8_t *profile);
void BLE_hci_power_control(uint8_t power_mode);
void BLE_gap_local_bd_addr(uint8_t *local_addr);
void BLE_push_event(uint8_t *packet, uint16_t size);
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


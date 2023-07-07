#ifndef BLE_DEFINED_H_
#define BLE_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>
#include <mrubyc.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Globals used in src/ble_{peripheral,central}.c */
extern mrbc_value singleton;
void BLE_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

/* Globals used in src/ble.c */
extern bool ble_heartbeat_on;

/* Globals used in ports/rp2040/ble.c */
extern uint16_t ble_heartbeat_period_ms;

void mrbc_init_class_BLE_Peripheral(void);
void mrbc_init_class_BLE_Central(void);

void BLE_hci_power_on(void);
void BLE_set_heartbeat_period_ms(uint16_t period_ms);

void BLE_gap_local_bd_addr(uint8_t *local_addr);

void BLE_push_event(uint8_t *packet, uint16_t size);

typedef struct {
  uint16_t att_handle;
  uint8_t *data;
  uint16_t size;
} BLE_read_value_t;

int BLE_write_data(uint16_t att_handle, const uint8_t *data, uint16_t size);
int BLE_read_data(BLE_read_value_t *read_value);


#ifdef __cplusplus
}
#endif

#endif /* BLE_DEFINED_H_ */


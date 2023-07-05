#ifndef BLE_DEFINED_H_
#define BLE_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Globals used in src/ble.c */
extern uint8_t packet_event_type;
extern bool ble_heartbeat_on;

/* Globals used in ports/rp2040/ble.c */
extern uint16_t heartbeat_period_ms;
extern uint8_t packet_event_state;

void mrbc_init_class_BLE_Peripheral(void);
void mrbc_init_class_BLE_Central(void);

void BLE_hci_power_on(void);
void BLE_set_heartbeat_period_ms(uint16_t period_ms);

void BLE_gap_local_bd_addr(uint8_t *local_addr);

#ifdef __cplusplus
}
#endif

#endif /* BLE_DEFINED_H_ */



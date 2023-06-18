#ifndef BLE_DEFINED_H_
#define BLE_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int BLE_init(void);
void BLE_hci_power_on(void);

uint8_t BLE_packet_event(void);
void BLE_down_packet_flag(void);
void BLE_advertise(void);
void BLE_enable_le_notification(void);
void BLE_notify(void);
void BLE_gap_local_bd_addr(uint8_t *local_addr);

#ifdef __cplusplus
}
#endif

#endif /* BLE_DEFINED_H_ */


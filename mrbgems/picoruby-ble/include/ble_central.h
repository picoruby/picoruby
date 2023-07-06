#ifndef BLE_CENTRAL_DEFINED_H_
#define BLE_CENTRAL_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t packet_event_type;
extern bool ble_heartbeat_on;

void CentralPushEvent(uint8_t event_type, uint8_t *packet, uint16_t size);

int BLE_central_init(void);
void BLE_central_start_scan(void);
int BLE_central_packet_event_state(void);

uint16_t BLE_central_get_packet(uint8_t *packet);

#ifdef __cplusplus
}
#endif

#endif /* BLE_CENTRAL_DEFINED_H_ */



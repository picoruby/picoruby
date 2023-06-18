#ifndef BLE_DEFINED_H_
#define BLE_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int BLE_init(void);
void BLE_start(void);

int BLE_packet_event(void);
void BLE_down_packet_flag(void);
void BLE_advertise(void);
void BLE_enable_le_notification(void);
void BLE_notify(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_DEFINED_H_ */


#ifndef BLE_CENTRAL_DEFINED_H_
#define BLE_CENTRAL_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bool ble_heartbeat_on;

int BLE_central_init(void);
void BLE_central_start_scan(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_CENTRAL_DEFINED_H_ */



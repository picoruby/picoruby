#ifndef BLE_CENTRAL_DEFINED_H_
#define BLE_CENTRAL_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void BLE_central_resume_scan(void);
void BLE_central_suspend_scan(void);
uint8_t BLE_central_gap_connect(const uint8_t *addr, uint8_t addr_type);
void BLE_central_push_service(uint16_t start_group_handle, uint16_t end_group_handle, uint16_t uuid16, const uint8_t *uuid128);

#ifdef __cplusplus
}
#endif

#endif /* BLE_CENTRAL_DEFINED_H_ */


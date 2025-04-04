#ifndef BLE_CENTRAL_DEFINED_H_
#define BLE_CENTRAL_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void BLE_central_set_scan_params(uint8_t scan_type, uint16_t scan_interval, uint16_t scan_window, uint8_t scanning_filter_policy);
void BLE_central_start_scan(void);
void BLE_central_stop_scan(void);
uint8_t BLE_central_gap_connect(const uint8_t *addr, uint8_t addr_type);

#ifdef __cplusplus
}
#endif

#endif /* BLE_CENTRAL_DEFINED_H_ */


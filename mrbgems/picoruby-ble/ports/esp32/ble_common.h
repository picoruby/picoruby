#ifndef BLE_COMMON_DEFINED_H_
#define BLE_COMMON_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#include "host/ble_gap.h"
#include "host/ble_gatt.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint16_t con_handle;

int picoruby_ble_gap_event(struct ble_gap_event *event, void *arg);
void picoruby_ble_gatts_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
uint16_t picoruby_ble_handle_n2r(uint16_t nimble_handle);
uint16_t picoruby_ble_handle_r2n(uint16_t ruby_handle);
void picoruby_ble_synth_state_working(void);
void picoruby_ble_peripheral_rearm_adv(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_COMMON_DEFINED_H_ */

// ports/esp32/ble_common.h
//
// Port-internal shared declarations for the NimBLE-based ESP32 port.
//
// Strategy: mrblib (ble_central.rb etc.) parses BTstack-wire-format event
// packets and GattDatabase emits BTstack att_db blobs, so this port
// synthesizes BTstack-compatible packets from NimBLE callbacks and translates
// the blob into ble_gatt_svc_def tables. Nothing outside ports/esp32 changes.

#ifndef BLE_COMMON_DEFINED_H_
#define BLE_COMMON_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#include "host/ble_gap.h"
#include "host/ble_gatt.h"

#ifdef __cplusplus
extern "C" {
#endif

// Active connection handle (peripheral: the connected central; central: the
// connected peripheral). BLE_HS_CONN_HANDLE_NONE-like 0xffff when idle.
extern uint16_t con_handle;

// Shared GAP event callback (ble.c) used for advertising, scanning and
// connections; switches behavior on the current BLE role.
int picoruby_ble_gap_event(struct ble_gap_event *event, void *arg);

// ble_hs_cfg.gatts_register_cb: records NimBLE-assigned descriptor handles
// into the blob-handle map (characteristic value handles arrive via the
// val_handle out-pointers).
void picoruby_ble_gatts_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);

// Blob(Ruby) handle <-> NimBLE handle translation. Returns 0 when unmapped.
uint16_t picoruby_ble_handle_n2r(uint16_t nimble_handle);
uint16_t picoruby_ble_handle_r2n(uint16_t ruby_handle);

// Enqueue BTSTACK_EVENT_STATE(HCI_STATE_WORKING); re-sent on every power-on
// because Ruby's BLE#start waits for it each cycle.
void picoruby_ble_synth_state_working(void);

// ble_peripheral.c: restart advertising after a disconnect / failed connect
// if the app still wants it (BTstack advertising persisted across
// connections; NimBLE stops on connect).
void picoruby_ble_peripheral_rearm_adv(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_COMMON_DEFINED_H_ */

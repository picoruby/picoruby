// ports/esp32/nimble_owner.h
//
// NimBLE host lifecycle + event delivery for the picoruby-ble ESP32 port.
//
// The shared src/ layer exposes a single-slot packet mailbox (BLE_push_event)
// that Ruby polls every 100 ms, so events synthesized from NimBLE callbacks
// are queued here and drained one-per-tick by a 150 ms dispatch timer.

#ifndef PICORUBY_NIMBLE_OWNER_H_
#define PICORUBY_NIMBLE_OWNER_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*picoruby_nimble_setup_fn)(void);

// Start the NimBLE host once: nvs init, nimble_port_init, ble_hs_cfg wiring,
// setup() (GATT service registration window), host task spawn, then block
// until the host syncs (2 s timeout). Returns 0 on success, -1 on failure.
int picoruby_nimble_start(picoruby_nimble_setup_fn setup);

// Full teardown (adv/scan stop, nimble_port_stop/deinit). Safe when not started.
int picoruby_nimble_stop(void);

bool picoruby_nimble_started(void);
uint8_t picoruby_nimble_own_addr_type(void);

// Queue a synthesized BTstack-format event packet for paced delivery to Ruby.
// coalesce_adv: advertising reports replace a queued advertising report
// instead of piling up, and are dropped first on overflow.
void picoruby_nimble_enqueue_event(const uint8_t *pkt, uint16_t len, bool coalesce_adv);

// 1 s heartbeat tick (-> BLE_heartbeat) gate, driven by BLE_hci_power_control.
void picoruby_nimble_heartbeat_enable(bool enable);

#ifdef __cplusplus
}
#endif

#endif /* PICORUBY_NIMBLE_OWNER_H_ */

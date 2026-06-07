// ports/esp32/btstack_owner.h
#ifndef PICORUBY_BLE_ESP32_BTSTACK_OWNER_H
#define PICORUBY_BLE_ESP32_BTSTACK_OWNER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*picoruby_btstack_callback_t)(void *ctx);

// Idempotent. On first call, spawns a FreeRTOS task that runs btstack_init(),
// invokes `setup(ctx)` on the BTstack thread (post btstack_init, pre run_loop),
// then enters btstack_run_loop_execute. Returns to the caller after setup
// completes. Subsequent calls dispatch `setup(ctx)` via picoruby_btstack_run_sync.
//
// BTstack is not thread-safe; the setup callback must perform all l2cap_init /
// sm_init / att_server_init / hci_add_event_handler etc. so they run on the
// BTstack thread.
void picoruby_btstack_ensure_started(picoruby_btstack_callback_t setup, void *ctx);

// Synchronously runs `cb(ctx)` on the BTstack thread via
// btstack_run_loop_execute_on_main_thread. Blocks the caller until cb returns.
// picoruby_btstack_ensure_started must have been called first.
void picoruby_btstack_run_sync(picoruby_btstack_callback_t cb, void *ctx);

#ifdef __cplusplus
}
#endif

#endif

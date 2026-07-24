#include <stdbool.h>
#include "picoruby.h"
#include "../include/ble.h"
#include "../include/ble_central.h"

#include "../../picoruby-machine/include/machine.h"

/*
 * Workaround: To avoid deadlock
 * TODO: Maybe we need a critical section instead of these simple mutex
 */
/* SPIKE (2026-07-23) ruled out: adding `volatile` to these had zero effect on
 * real ESP32-S3 hardware (confirmed via a freshly rebuilt+reflashed binary,
 * distinct ELF hash) — BTSTACK_EVENT_STATE still never reaches Ruby despite
 * BLE_push_event completing. Cross-core memory visibility is not the cause;
 * reverted to plain bool. See docs/superpowers/handoff/
 * 2026-07-22-ble-role-coverage-verification-evidence.md. */
static bool packet_mutex = false;
static bool write_values_mutex = false;
static bool heatbeat_flag = false;
static bool packet_flag = false;

static uint8_t *packet = NULL;
static uint16_t packet_size = 0;

#if defined(PICORB_VM_MRUBY)

#include "mruby/ble.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/ble.c"

#endif

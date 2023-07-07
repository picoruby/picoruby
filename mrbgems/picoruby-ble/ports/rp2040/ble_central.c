#include <stdint.h>
#include <stdbool.h>

#include "../../include/ble.h"
#include "../../include/ble_central.h"

#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"
#include "pico/stdlib.h"

static btstack_timer_source_t heartbeat;
static btstack_packet_callback_registration_t hci_event_callback_registration;

static void
heartbeat_handler(struct btstack_timer_source *ts)
{
  ble_heartbeat_on = true;
  // Restart timer
  btstack_run_loop_set_timer(ts, heartbeat_period_ms);
  btstack_run_loop_add_timer(ts);
}

int
BLE_central_init(void)
{
  l2cap_init();
  sm_init();
  sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);

  // setup empty ATT server - only needed if LE Peripheral does ATT queries on its own, e.g. Android and iOS
  att_server_init(NULL, NULL, NULL);

  // inform about BTstack state
  hci_event_callback_registration.callback = &packet_handler;
  hci_add_event_handler(&hci_event_callback_registration);

  // set one-shot btstack timer
  heartbeat.process = &heartbeat_handler;
  btstack_run_loop_set_timer(&heartbeat, heartbeat_period_ms);
  btstack_run_loop_add_timer(&heartbeat);
  return 0;
}

void
BLE_central_start_scan(void){
  gap_set_scan_parameters(0, 0x0030, 0x0030);
  gap_start_scan();
}


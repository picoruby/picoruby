#include <stdint.h>
#include <stdbool.h>

#include "../../include/ble_central.h"

#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"
#include "pico/stdlib.h"

typedef enum {
    TC_OFF,
    TC_IDLE,
    TC_W4_SCAN_RESULT,
    TC_W4_CONNECT,
    TC_W4_SERVICE_RESULT,
    TC_W4_CHARACTERISTIC_RESULT,
    TC_W4_ENABLE_NOTIFICATIONS_COMPLETE,
    TC_W4_READY
} gc_state_t;

static gc_state_t state = TC_OFF;

static uint8_t packet_event_state = 0;
static uint16_t heartbeat_period_ms = 1000;

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

static uint8_t *last_packet = NULL;
static uint16_t last_packet_size = 0;

static void
packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
  last_packet = packet;
  last_packet_size = size;
  if (packet_type != HCI_EVENT_PACKET) return;
  uint8_t _type = hci_event_packet_get_type(packet);
  switch (_type) {
    /*
     * Ignore these events so that we can get a DISCONNECTION event.
     */
    case HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS: // 0x13
    case BTSTACK_EVENT_NR_CONNECTIONS_CHANGED:  // 0x61
    case HCI_EVENT_TRANSPORT_PACKET_SENT:       // 0x6e
    case HCI_EVENT_COMMAND_COMPLETE:            // 0x0e
      break;
    default:
      packet_event_type = _type;
  }
  packet_event_state = btstack_event_state_get_state(packet);
}

int
BLE_central_packet_event_state(void)
{
  return packet_event_state;
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
  state = TC_W4_SCAN_RESULT;
  gap_set_scan_parameters(0, 0x0030, 0x0030);
  gap_start_scan();
}

void
BLE_central_show_packet(void)
{
  if (last_packet == NULL) return;
  printf("Packet: ");
  for (int i = 0; i < last_packet_size; i++) {
    printf("%02x ", last_packet[i]);
  }
  printf("\n");
}

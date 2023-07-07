#include <stdint.h>
#include <stdbool.h>

#include "../../include/ble.h"

#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"
#include "pico/stdlib.h"

uint16_t heartbeat_period_ms = 1000;

void BLE_hci_power_on(void)
{
  hci_power_control(HCI_POWER_ON);
}

void
BLE_set_heartbeat_period_ms(uint16_t period_ms)
{
  heartbeat_period_ms = period_ms;
}

void
BLE_gap_local_bd_addr(uint8_t *local_addr)
{
  gap_local_bd_addr(local_addr);
}

void
packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
  if (packet_type != HCI_EVENT_PACKET) return;
//  uint8_t _type = hci_event_packet_get_type(packet);
//  switch (_type) {
//    /*
//     * Ignore these events so that we can get a DISCONNECTION event.
//     */
//    case HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS: // 0x13
//    case BTSTACK_EVENT_NR_CONNECTIONS_CHANGED:  // 0x61
//    case HCI_EVENT_TRANSPORT_PACKET_SENT:       // 0x6e
//    case HCI_EVENT_COMMAND_COMPLETE:            // 0x0e
//      break;
//    default:
//      BLE_push_event(_type, packet, size);
//  }
  BLE_push_event(packet, size);
}


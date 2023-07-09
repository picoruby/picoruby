#include <stdint.h>
#include <stdbool.h>

#include "../../include/ble.h"

#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"
#include "pico/stdlib.h"

#include "ble_common.h"

btstack_packet_callback_registration_t ble_hci_event_callback_registration;

uint16_t
att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
  UNUSED(connection_handle);
  BLE_read_value_t read_value = { .att_handle = att_handle, .data = NULL, .size = 0 };
  if (BLE_read_data(&read_value) < 0) return 0;
  return att_read_callback_handle_blob(
           (const uint8_t *)read_value.data,
           read_value.size,
           offset,
           buffer,
           buffer_size
         );
}

int
att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
  UNUSED(transaction_mode);
  UNUSED(offset);

  if (0 == BLE_write_data(att_handle, (const uint8_t *)buffer, buffer_size)) {
    con_handle = connection_handle;
  }
  return 0;
}

int
BLE_init(const uint8_t *profile_data)
{
  l2cap_init();
  sm_init();

  if (profile_data) {
    att_server_init(profile_data, att_read_callback, att_write_callback);
    att_server_register_packet_handler(BLE_packet_handler);
  } else {
    att_server_init(NULL, NULL, NULL);
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
  }

  // inform about BTstack state
  ble_hci_event_callback_registration.callback = &BLE_packet_handler;
  hci_add_event_handler(&ble_hci_event_callback_registration);

  return 0;
}

void
BLE_hci_power_control(uint8_t power_mode)
{
  hci_power_control(power_mode);
}

void
BLE_gap_local_bd_addr(uint8_t *local_addr)
{
  gap_local_bd_addr(local_addr);
}

void
BLE_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
  if (packet_type != HCI_EVENT_PACKET) return;
  uint8_t _type = hci_event_packet_get_type(packet);
  switch (_type) {
    /*
     * Ignore these events not to overflow the queue
     */
    case HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS: // 0x13
    case BTSTACK_EVENT_NR_CONNECTIONS_CHANGED:  // 0x61
    case HCI_EVENT_TRANSPORT_PACKET_SENT:       // 0x6e
    case HCI_EVENT_COMMAND_COMPLETE:            // 0x0e
      break;
    default:
      BLE_push_event(packet, size);
      break;
  }
}


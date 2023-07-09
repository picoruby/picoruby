#include <stdint.h>
#include <stdbool.h>

#include "../../include/ble.h"
#include "../../include/ble_peripheral.h"

#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"
#include "pico/stdlib.h"

#include "ble_common.h"

hci_con_handle_t con_handle;

void
BLE_peripheral_advertise(uint8_t *adv_data, uint8_t adv_data_len)
{
  // setup advertisements
  uint16_t adv_int_min = 800;
  uint16_t adv_int_max = 800;
  uint8_t adv_type = 0;
  bd_addr_t null_addr;
  memset(null_addr, 0, 6);
  gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
  gap_advertisements_set_data(adv_data_len, adv_data);
  gap_advertisements_enable(1);
}

void
BLE_peripheral_notify(uint16_t att_handle)
{
  BLE_read_value_t read_value = { .att_handle = att_handle, .data = NULL, .size = 0 };
  if (BLE_read_data(&read_value) < 0) return;
  if (read_value.size == 0) return;
  att_server_notify(con_handle, att_handle, read_value.data, read_value.size);
}

void
BLE_peripheral_request_can_send_now_event(void)
{
  att_server_request_can_send_now_event(con_handle);
}


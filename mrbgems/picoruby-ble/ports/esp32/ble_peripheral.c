#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "../../include/ble.h"
#include "../../include/ble_peripheral.h"

#include "btstack.h"

#include "ble_common.h"
#include "btstack_owner.h"

hci_con_handle_t con_handle;

// All BTstack APIs are called from Ruby thread via Ruby's packet_callback
// (which itself runs on Ruby thread after pop_packet). Wrap each call so it
// executes on the BTstack thread.

typedef struct {
  uint8_t *adv_data;
  uint8_t adv_data_len;
  bool connectable;
} advertise_ctx_t;

static void
peripheral_advertise_on_btstack_thread(void *ctx_p)
{
  advertise_ctx_t *p = (advertise_ctx_t *)ctx_p;
  uint16_t adv_int_min = 800;
  uint16_t adv_int_max = 800;
  uint8_t adv_type = p->connectable ? 0 : 2;
  uint8_t channel_map = 0x07;
  uint8_t filter_policy = 0x00;
  bd_addr_t null_addr;
  memset(null_addr, 0, 6);
  gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, channel_map, filter_policy);
  gap_advertisements_set_data(p->adv_data_len, p->adv_data);
  gap_advertisements_enable(1);
}

void
BLE_peripheral_advertise(uint8_t *adv_data, uint8_t adv_data_len, bool connectable)
{
  advertise_ctx_t ctx = { .adv_data = adv_data, .adv_data_len = adv_data_len, .connectable = connectable };
  picoruby_btstack_run_sync(peripheral_advertise_on_btstack_thread, &ctx);
}

static void
peripheral_stop_advertise_on_btstack_thread(void *ctx_p)
{
  (void)ctx_p;
  gap_advertisements_enable(0);
}

void
BLE_peripheral_stop_advertise(void)
{
  picoruby_btstack_run_sync(peripheral_stop_advertise_on_btstack_thread, NULL);
}

static void
peripheral_notify_on_btstack_thread(void *ctx_p)
{
  uint16_t att_handle = (uint16_t)(uintptr_t)ctx_p;
  BLE_read_value_t read_value = { .att_handle = att_handle, .data = NULL, .size = 0 };
  if (BLE_read_data(&read_value) < 0) return;
  if (read_value.size == 0) return;
  att_server_notify(con_handle, att_handle, read_value.data, read_value.size);
}

void
BLE_peripheral_notify(uint16_t att_handle)
{
  picoruby_btstack_run_sync(peripheral_notify_on_btstack_thread, (void *)(uintptr_t)att_handle);
}

static void
peripheral_request_can_send_now_on_btstack_thread(void *ctx_p)
{
  (void)ctx_p;
  att_server_request_can_send_now_event(con_handle);
}

void
BLE_peripheral_request_can_send_now_event(void)
{
  picoruby_btstack_run_sync(peripheral_request_can_send_now_on_btstack_thread, NULL);
}

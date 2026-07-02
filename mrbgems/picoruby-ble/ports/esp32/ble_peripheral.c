// ports/esp32/ble_peripheral.c
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "../../include/ble.h"
#include "../../include/ble_peripheral.h"

#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs_mbuf.h"
#include "esp_log.h"

#include "ble_common.h"
#include "nimble_owner.h"

static const char *TAG = "prb_ble";

uint16_t con_handle = 0xffff;

#define ADV_INTERVAL_0625MS 800 // 500 ms, matching the rp2040/BTstack port

static uint8_t adv_data_buf[31];
static uint8_t adv_data_len = 0;
static bool adv_connectable = false;
static bool adv_wanted = false;

static int
adv_start(void)
{
  struct ble_gap_adv_params params;
  memset(&params, 0, sizeof(params));
  params.conn_mode = adv_connectable ? BLE_GAP_CONN_MODE_UND : BLE_GAP_CONN_MODE_NON;
  params.disc_mode = BLE_GAP_DISC_MODE_GEN;
  params.itvl_min = ADV_INTERVAL_0625MS;
  params.itvl_max = ADV_INTERVAL_0625MS;
  int rc = ble_gap_adv_set_data(adv_data_buf, adv_data_len);
  if (rc == 0) {
    rc = ble_gap_adv_start(picoruby_nimble_own_addr_type(), NULL, BLE_HS_FOREVER,
                           &params, picoruby_ble_gap_event, NULL);
  }
  if (rc != 0 && rc != BLE_HS_EALREADY) {
    ESP_LOGW(TAG, "adv start failed: %d", rc);
  }
  return rc;
}

void
BLE_peripheral_advertise(uint8_t *adv_data, uint8_t adv_data_len_in, bool connectable)
{
  if (adv_data_len_in > sizeof(adv_data_buf)) adv_data_len_in = sizeof(adv_data_buf);
  memcpy(adv_data_buf, adv_data, adv_data_len_in);
  adv_data_len = adv_data_len_in;
  adv_connectable = connectable;
  adv_wanted = true;
  ble_gap_adv_stop(); // restart requires stop; harmless when idle
  adv_start();
}

void
BLE_peripheral_stop_advertise(void)
{
  adv_wanted = false;
  ble_gap_adv_stop();
}

// NimBLE stops advertising when a connection forms (BTstack kept it enabled):
// restart on disconnect / adv-complete so the device stays reachable.
void
picoruby_ble_peripheral_rearm_adv(void)
{
  if (adv_wanted) adv_start();
}

void
BLE_peripheral_notify(uint16_t att_handle)
{
  BLE_read_value_t read_value = { .att_handle = att_handle, .data = NULL, .size = 0 };
  if (BLE_read_data(&read_value) < 0) return;
  if (read_value.size == 0) return;
  uint16_t nimble_handle = picoruby_ble_handle_r2n(att_handle);
  if (nimble_handle == 0 || con_handle == 0xffff) return;
  struct os_mbuf *om = ble_hs_mbuf_from_flat(read_value.data, read_value.size);
  if (om == NULL) return;
  ble_gatts_notify_custom(con_handle, nimble_handle, om); // consumes om
}

void
BLE_peripheral_request_can_send_now_event(void)
{
  // BTstack flow-control handshake; NimBLE needs no permission to notify,
  // so grant immediately.
  uint8_t p[4] = { 0xB7 /* ATT_EVENT_CAN_SEND_NOW */, 2, 0, 0 };
  p[2] = con_handle & 0xff;
  p[3] = con_handle >> 8;
  picoruby_nimble_enqueue_event(p, sizeof(p), false);
}

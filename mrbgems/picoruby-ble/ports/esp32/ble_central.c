#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "../../include/ble.h"
#include "../../include/ble_central.h"

#include "host/ble_hs.h"
#include "host/ble_gap.h"

#include "ble_common.h"
#include "nimble_owner.h"

static struct ble_gap_disc_params scan_params = {
  .itvl = 0x60,
  .window = 0x30,
  .passive = 1,
};

/**
 * @param scan_type 0 = passive, 1 = active
 * @param scan_interval range 0x0004..0x4000, unit 0.625 ms
 * @param scan_window range 0x0004..0x4000, unit 0.625 ms
 * @param scanning_filter_policy 0 = all devices, 1 = all from whitelist
 */
void
BLE_central_set_scan_params(uint8_t scan_type, uint16_t scan_interval, uint16_t scan_window, uint8_t scanning_filter_policy)
{
  scan_params.itvl = scan_interval;
  scan_params.window = scan_window;
  scan_params.passive = (scan_type == 0);
  scan_params.filter_policy = scanning_filter_policy;
  scan_params.filter_duplicates = 0;
}

void
BLE_central_start_scan(void)
{
  ble_gap_disc(picoruby_nimble_own_addr_type(), BLE_HS_FOREVER,
              &scan_params, picoruby_ble_gap_event, NULL);
}

void
BLE_central_stop_scan(void)
{
  ble_gap_disc_cancel();
}

uint8_t
BLE_central_gap_connect(const uint8_t *addr, uint8_t addr_type)
{
  if (ble_gap_disc_active()) {
    ble_gap_disc_cancel();
  }

  ble_addr_t peer;
  peer.type = addr_type;
  for (int i = 0; i < 6; i++) peer.val[i] = addr[5 - i];

  struct ble_gap_conn_params params = {
    .scan_itvl = 60000 / 625,
    .scan_window = 30000 / 625,
    .itvl_min = 10000 / 1250,
    .itvl_max = 30000 / 1250,
    .latency = 4,
    .supervision_timeout = 7200 / 10,
    .min_ce_len = 10000 / 625,
    .max_ce_len = 30000 / 625,
  };

  int rc = ble_gap_connect(picoruby_nimble_own_addr_type(), &peer, 30000,
                           &params, picoruby_ble_gap_event, NULL);
  if (rc == 0) return 0;
  return (rc & 0xff) ? (uint8_t)rc : 0xff;
}

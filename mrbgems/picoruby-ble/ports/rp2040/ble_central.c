#include <stdint.h>
#include <stdbool.h>

#include "../../include/ble.h"
#include "../../include/ble_central.h"

#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"
#include "pico/stdlib.h"

#include "ble_common.h"

void
BLE_central_resume_scan(void){
  gap_set_scan_parameters(0, 0x0030, 0x0030);
  gap_resume_scan();
}

void
BLE_central_suspend_scan(void){
  gap_suspend_scan();
}

uint8_t
BLE_central_gap_connect(const uint8_t *addr, uint8_t addr_type)
{

  uint16_t conn_scan_interval = 60000 / 625;
  uint16_t conn_scan_window = 30000 / 625;
  uint16_t conn_interval_min = 10000 / 1250;
  uint16_t conn_interval_max = 30000 / 1250;
  uint16_t conn_latency = 4;
  uint16_t supervision_timeout = 7200 / 10; // default = 720
  uint16_t min_ce_length = 10000 / 625;
  uint16_t max_ce_length = 30000 / 625;

  gap_set_connection_parameters(conn_scan_interval, conn_scan_window, conn_interval_min, conn_interval_max, conn_latency, supervision_timeout, min_ce_length, max_ce_length);

  return gap_connect(addr, (bd_addr_type_t)addr_type);
}


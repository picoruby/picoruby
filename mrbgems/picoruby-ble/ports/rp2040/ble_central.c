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
BLE_central_start_scan(void){
  gap_set_scan_parameters(0, 0x0030, 0x0030);
  gap_start_scan();
}


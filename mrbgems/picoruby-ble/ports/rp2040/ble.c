#include <stdint.h>
#include <stdbool.h>

#include "../../include/ble.h"

#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"
#include "pico/stdlib.h"

uint16_t heartbeat_period_ms = 1000;
uint8_t packet_event_state = 0;

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


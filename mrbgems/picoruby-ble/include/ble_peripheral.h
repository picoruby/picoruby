#ifndef BLE_PERIPHERAL_DEFINED_H_
#define BLE_PERIPHERAL_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint16_t att_handle;
  uint8_t *data;
  uint16_t size;
} BLE_read_value;

int PeripheralWriteData(uint16_t att_handle, const uint8_t *data, uint16_t size);
int PeripheralReadData(BLE_read_value *read_value);

extern uint8_t packet_event_type;
extern bool ble_heartbeat_on;

void BLE_peripheral_set_heartbeat_period_ms(uint16_t period_ms);
int BLE_peripheral_init(const uint8_t *profile);

void BLE_peripheral_advertise(uint8_t *adv_data, uint8_t adv_data_len);
void BLE_peripheral_notify(uint16_t att_handle);
void BLE_peripheral_gap_local_bd_addr(uint8_t *local_addr);

void BLE_peripheral_request_can_send_now_event(void);
void BLE_peripheral_cyw43_arch_gpio_put(uint8_t pin, uint8_t value);

#ifdef __cplusplus
}
#endif

#endif /* BLE_PERIPHERAL_DEFINED_H_ */


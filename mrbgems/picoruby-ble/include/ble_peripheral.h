#ifndef BLE_PERIPHERAL_DEFINED_H_
#define BLE_PERIPHERAL_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void BLE_peripheral_advertise(uint8_t *adv_data, uint8_t adv_data_len);
void BLE_peripheral_notify(uint16_t att_handle);

void BLE_peripheral_request_can_send_now_event(void);
void BLE_peripheral_cyw43_arch_gpio_put(uint8_t pin, uint8_t value);

#ifdef __cplusplus
}
#endif

#endif /* BLE_PERIPHERAL_DEFINED_H_ */


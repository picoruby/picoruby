#ifndef BLE_DEFINED_H_
#define BLE_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void mrbc_init_class_BLE_Peripheral(void);
void mrbc_init_class_BLE_Central(void);

void BLE_hci_power_on(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_DEFINED_H_ */



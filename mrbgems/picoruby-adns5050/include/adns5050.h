#ifndef ADNS5050_DEFINED_H_
#define ADNS5050_DEFINED_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ADNS5050_init(uint8_t sclk_pin, uint8_t sdio_pin, uint8_t ncs_pin);
int ADNS5050_read8(uint8_t addr);
void ADNS5050_write8(uint8_t addr, uint8_t data);

#ifdef __cplusplus
}
#endif

#endif /* ADNS5050_DEFINED_H_ */


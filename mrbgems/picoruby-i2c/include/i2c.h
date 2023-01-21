#ifndef I2C_DEFINED_H_
#define I2C_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void I2C_gpio_init(uint8_t, uint32_t, uint8_t, uint8_t);
int I2C_read_blocking(int, uint8_t, uint8_t*, size_t, bool);
int I2C_write_blocking(int, uint8_t, uint8_t*, size_t, bool);

#ifdef __cplusplus
}
#endif

#endif /* I2C_DEFINED_H_ */


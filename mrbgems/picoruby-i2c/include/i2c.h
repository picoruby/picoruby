#ifndef I2C_DEFINED_H_
#define I2C_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PICORUBY_I2C_RP2040_I2C0      0
#define PICORUBY_I2C_RP2040_I2C1      1

void I2C_gpio_init(uint8_t, uint32_t, uint8_t, uint8_t);
int I2C_read_timeout_us(int, uint8_t, uint8_t*, size_t, bool, uint32_t);
int I2C_write_timeout_us(int, uint8_t, uint8_t*, size_t, bool, uint32_t);

#ifdef __cplusplus
}
#endif

#endif /* I2C_DEFINED_H_ */


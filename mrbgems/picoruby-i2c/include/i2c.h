#ifndef I2C_DEFINED_H_
#define I2C_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PICORUBY_I2C_RP2040_I2C0      0
#define PICORUBY_I2C_RP2040_I2C1      1

typedef enum {
 ERROR_NONE              =  0,
 ERROR_INVALID_UNIT      = -1,
} i2c_status_t;

int I2C_unit_name_to_unit_num(const char *);
i2c_status_t I2C_gpio_init(int, uint32_t, int8_t, int8_t);
int I2C_read_timeout_us(int, uint8_t, uint8_t*, size_t, bool, uint32_t);
int I2C_write_timeout_us(int, uint8_t, uint8_t*, size_t, bool, uint32_t);

#ifdef __cplusplus
}
#endif

#endif /* I2C_DEFINED_H_ */


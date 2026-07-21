#ifndef I2C_DEFINED_H_
#define I2C_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PICORB_I2C_RP2040_I2C0      0
#define PICORB_I2C_RP2040_I2C1      1

typedef enum {
 I2C_ERROR_NONE          =  0,
 I2C_ERROR_INVALID_UNIT  = -1,
 I2C_ERROR_UNIT_MISMATCH = -2,
 I2C_ERROR_UNDETERMINED  = -3,
} i2c_status_t;

int I2C_unit_name_to_unit_num(const char *);
/*
 * Resolve the unit number from the unit name and the SDA/SCL pins.
 * On RP2 the unit can be inferred from the pins when the name is empty, and a
 * mismatch between name and pins (or between pins) yields a negative error code.
 * On other platforms this falls back to I2C_unit_name_to_unit_num().
 */
int I2C_resolve_unit_num(const char *unit_name, int sda_pin, int scl_pin);
int I2C_unit_num_from_pins(const char *unit_name, int sda_pin, int scl_pin);
i2c_status_t I2C_gpio_init(int, uint32_t, int8_t, int8_t);
int I2C_read_timeout_us(int, uint8_t, uint8_t*, size_t, bool, uint32_t);
int I2C_write_timeout_us(int, uint8_t, uint8_t*, size_t, bool, uint32_t);

#ifdef __cplusplus
}
#endif

#endif /* I2C_DEFINED_H_ */


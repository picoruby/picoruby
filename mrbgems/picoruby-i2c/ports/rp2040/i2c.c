#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"

#include "../../include/i2c.h"

#define UNIT_SELECT() \
  do { \
    switch (unit_num) { \
      case PICORUBY_I2C_RP2040_I2C0: { unit = i2c0; break; } \
      case PICORUBY_I2C_RP2040_I2C1: { unit = i2c1; break; } \
      default: { return I2C_ERROR_INVALID_UNIT; } \
    } \
  } while (0)

int
I2C_read_timeout_us(int unit_num, uint8_t addr, uint8_t *dst, size_t len, bool nostop, uint32_t timeout)
{
  i2c_inst_t *unit;
  UNIT_SELECT();
  return i2c_read_timeout_us(unit, addr, dst, len, nostop, timeout);
}

int
I2C_write_timeout_us(int unit_num, uint8_t addr, uint8_t *src, size_t len, bool nostop, uint32_t timeout)
{
  i2c_inst_t *unit;
  UNIT_SELECT();
  return i2c_write_timeout_us(unit, addr, src, len, nostop, timeout);
}

int
I2C_unit_name_to_unit_num(const char *unit_name)
{
  if (strcmp(unit_name, "RP2040_I2C0") == 0) {
    return PICORUBY_I2C_RP2040_I2C0;
  } else if (strcmp(unit_name, "RP2040_I2C1") == 0) {
    return PICORUBY_I2C_RP2040_I2C1;
  } else {
    return I2C_ERROR_INVALID_UNIT;
  }
}

i2c_status_t
I2C_gpio_init(int unit_num, uint32_t frequency, int8_t sda_pin, int8_t scl_pin)
{
  i2c_inst_t *unit;
  UNIT_SELECT();
  i2c_init(unit, frequency);

  if (sda_pin < 0) { sda_pin = PICO_DEFAULT_I2C_SDA_PIN; }
  if (scl_pin < 0) { sda_pin = PICO_DEFAULT_I2C_SCL_PIN; }
  gpio_set_function(sda_pin, GPIO_FUNC_I2C);
  gpio_set_function(scl_pin, GPIO_FUNC_I2C);
  gpio_pull_up(sda_pin);
  gpio_pull_up(scl_pin);

  return I2C_ERROR_NONE;
}


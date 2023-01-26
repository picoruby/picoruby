#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"

#include "../../include/i2c.h"

#define UNIT_SELECT() do { \
    (unit_num == PICORUBY_I2C_RP2040_I2C0) ? (unit = i2c0) : (unit = i2c1); \
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

void
I2C_gpio_init(uint8_t unit_num, uint32_t frequency, uint8_t sda_pin, uint8_t scl_pin)
{
  i2c_inst_t *unit;
  UNIT_SELECT();
  i2c_init(unit, frequency);
  gpio_set_function(sda_pin, GPIO_FUNC_I2C);
  gpio_set_function(scl_pin, GPIO_FUNC_I2C);
  gpio_pull_up(sda_pin);
  gpio_pull_up(scl_pin);
}


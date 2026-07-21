#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"

#include "../../include/i2c.h"

#define UNIT_SELECT() \
  do { \
    switch (unit_num) { \
      case PICORB_I2C_RP2040_I2C0: { unit = i2c0; break; } \
      case PICORB_I2C_RP2040_I2C1: { unit = i2c1; break; } \
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
    return PICORB_I2C_RP2040_I2C0;
  } else if (strcmp(unit_name, "RP2040_I2C1") == 0) {
    return PICORB_I2C_RP2040_I2C1;
  } else {
    return I2C_ERROR_INVALID_UNIT;
  }
}

/*
 * RP2040 GPIO function-select table (valid for RP2350 QFN-60 as well).
 * Each array lists the GPIOs usable as the given I2C signal, terminated by -1.
 */
static const int8_t i2c0_sda_pins[] = {0, 4, 8, 12, 16, 20, 24, 28, -1};
static const int8_t i2c1_sda_pins[] = {2, 6, 10, 14, 18, 22, 26, -1};
static const int8_t i2c0_scl_pins[] = {1, 5, 9, 13, 17, 21, 25, 29, -1};
static const int8_t i2c1_scl_pins[] = {3, 7, 11, 15, 19, 23, 27, -1};

/* Return the unit that owns pin for the given signal, or -1 if none. */
static int
pin_to_unit(const int8_t *unit0_pins, const int8_t *unit1_pins, int pin)
{
  int i = 0;
  while (unit0_pins[i] != -1) {
    if (unit0_pins[i] == pin) {
      return PICORB_I2C_RP2040_I2C0;
    }
    i++;
  }
  i = 0;
  while (unit1_pins[i] != -1) {
    if (unit1_pins[i] == pin) {
      return PICORB_I2C_RP2040_I2C1;
    }
    i++;
  }
  return -1;
}

int
I2C_unit_num_from_pins(const char *unit_name, int sda_pin, int scl_pin)
{
  int candidate = -1;
  if (0 <= sda_pin) {
    int u = pin_to_unit(i2c0_sda_pins, i2c1_sda_pins, sda_pin);
    if (u < 0) {
      return I2C_ERROR_UNIT_MISMATCH;
    }
    candidate = u;
  }
  if (0 <= scl_pin) {
    int u = pin_to_unit(i2c0_scl_pins, i2c1_scl_pins, scl_pin);
    if (u < 0) {
      return I2C_ERROR_UNIT_MISMATCH;
    }
    if (0 <= candidate && candidate != u) {
      return I2C_ERROR_UNIT_MISMATCH;
    }
    candidate = u;
  }
  if (unit_name && unit_name[0] != '\0') {
    int name_unit = I2C_unit_name_to_unit_num(unit_name);
    if (name_unit < 0) {
      return I2C_ERROR_INVALID_UNIT;
    }
    if (0 <= candidate && candidate != name_unit) {
      return I2C_ERROR_UNIT_MISMATCH;
    }
    return name_unit;
  }
  if (candidate < 0) {
    return I2C_ERROR_UNDETERMINED;
  }
  return candidate;
}

i2c_status_t
I2C_gpio_init(int unit_num, uint32_t frequency, int8_t sda_pin, int8_t scl_pin)
{
  i2c_inst_t *unit;
  UNIT_SELECT();
  i2c_init(unit, frequency);

  if (sda_pin < 0) { sda_pin = PICO_DEFAULT_I2C_SDA_PIN; }
  if (scl_pin < 0) { scl_pin = PICO_DEFAULT_I2C_SCL_PIN; }
  gpio_set_function(sda_pin, GPIO_FUNC_I2C);
  gpio_set_function(scl_pin, GPIO_FUNC_I2C);
  gpio_pull_up(sda_pin);
  gpio_pull_up(scl_pin);

  return I2C_ERROR_NONE;
}


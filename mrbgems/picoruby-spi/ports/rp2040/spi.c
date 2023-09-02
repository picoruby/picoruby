#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"

#include "../../include/spi.h"

#define UNIT_SELECT() \
  do { \
    switch (unit_num) { \
      case PICORUBY_SPI_SOFTWARE:    { unit = NULL; break; } \
      case PICORUBY_SPI_RP2040_SPI0: { unit = spi0; break; } \
      case PICORUBY_SPI_RP2040_SPI1: { unit = spi1; break; } \
      default: { return ERROR_INVALID_UNIT; } \
    } \
  } while (0)

// For software SPI (workaround)
static uint8_t sck = -1;
static uint8_t copi = -1;
static uint8_t cipo = -1;

static int
software_read_blocking(uint8_t *dst, size_t len)
{
  uint8_t temp = 0;
  int n;
  for (int reg = 0; reg < len; reg++) {
    temp = reg;
    gpio_put(sck, 0);
    gpio_set_dir(copi, GPIO_OUT);
    for (n = 0; n < 8; n++) {
      gpio_put(sck, 0);
      busy_wait_us_32(1);
      gpio_put(copi, (temp & 0x80) != 0);
      temp <<= 1;
      gpio_put(sck, 1);
      busy_wait_us_32(1);
    }
    temp = 0;
    gpio_set_dir(cipo, GPIO_IN);
    for (n = 0; n < 8; n++) {
      busy_wait_us_32(1);
      gpio_put(sck, 0);
      busy_wait_us_32(1);
      temp <<= 1;
      temp |= gpio_get(cipo);
      busy_wait_us_32(20);
      gpio_put(sck, 1);
    }
    dst[reg] = temp;
  }
  return len;
}

static int
software_write_blocking(uint8_t *src, size_t len)
{
  uint8_t temp = 0;
  int n;
  gpio_set_dir(copi, GPIO_OUT);
  for (int reg = 0; reg < len; reg++) {
    temp = reg;
    gpio_put(sck, 0);
    for (n = 0; n < 8; n++) {
      gpio_put(sck, 0);
      busy_wait_us_32(1);
      gpio_put(copi, (temp & 0x80) != 0);
      temp <<= 1;
      gpio_put(sck, 1);
      busy_wait_us_32(1);
    }
    temp = src[reg];
    for (n = 0; n < 8; n++) {
      gpio_put(sck, 0);
      busy_wait_us_32(1);
      gpio_put(copi, (temp & 0x80) != 0);
      temp <<= 1;
      gpio_put(sck, 1);
      busy_wait_us_32(1);
    }
  }
  return len;
}

int
SPI_read_blocking(int unit_num, uint8_t *dst, size_t len, uint8_t repeated_tx_data)
{
  spi_inst_t *unit;
  UNIT_SELECT();
  if (PICORUBY_SPI_SOFTWARE < unit) {
    return spi_read_blocking(unit, repeated_tx_data, dst, len);
  } else {
    return software_read_blocking(dst, len);
  }
}

int
SPI_write_blocking(int unit_num, uint8_t *src, size_t len)
{
  spi_inst_t *unit;
  UNIT_SELECT();
  if (PICORUBY_SPI_SOFTWARE < unit) {
    return spi_write_blocking(unit, src, len);
  } else {
    return software_write_blocking(src, len);
  }
}

int
SPI_transfer(int unit_num, uint8_t *txdata, uint8_t *rxdata, size_t len)
{
  spi_inst_t *unit;
  UNIT_SELECT();
  if (PICORUBY_SPI_SOFTWARE < unit) {
    return spi_write_read_blocking(unit, txdata, rxdata, len);
  } else {
    return ERROR_NOT_IMPLEMENTED;
  }
}

int
SPI_unit_name_to_unit_num(const char *unit_name)
{
  if (strcmp(unit_name, "NONE") == 0) {
    return PICORUBY_SPI_SOFTWARE;
  } else if (strcmp(unit_name, "RP2040_SPI0") == 0) {
    return PICORUBY_SPI_RP2040_SPI0;
  } else if (strcmp(unit_name, "RP2040_SPI1") == 0) {
    return PICORUBY_SPI_RP2040_SPI1;
  } else {
    return ERROR_INVALID_UNIT;
  }
}

spi_status_t
SPI_gpio_init(int unit_num, uint32_t frequency, int8_t sck_pin, int8_t cipo_pin, int8_t copi_pin, uint8_t mode, uint8_t first_bit, uint8_t data_bits)
{
  spi_inst_t *unit;
  UNIT_SELECT();

  if (sck_pin  < 0) { sck_pin  = PICO_DEFAULT_SPI_SCK_PIN; }
  if (cipo_pin < 0) { cipo_pin = PICO_DEFAULT_SPI_RX_PIN; }
  if (copi_pin < 0) { copi_pin = PICO_DEFAULT_SPI_TX_PIN; }

  if (unit) {
    spi_init(unit, frequency);
    gpio_set_function(sck_pin, GPIO_FUNC_SPI);
    gpio_set_function(cipo_pin, GPIO_FUNC_SPI);
    gpio_set_function(copi_pin, GPIO_FUNC_SPI);

    /* mode */
    if (first_bit != SPI_MSB_FIRST) {
      /* RP2040 supports only MSB first */
      return ERROR_INVALID_FIRST_BIT;
    }
    spi_cpol_t cpol;
    spi_cpha_t cpha;
    switch (mode) {
      case 0: { cpol = 0; cpha = 0; break; }
      case 1: { cpol = 0; cpha = 1; break; }
      case 2: { cpol = 1; cpha = 0; break; }
      case 3: { cpol = 1; cpha = 1; break; }
      default: { return ERROR_INVALID_MODE; }
    }
    spi_set_format(unit, data_bits, cpol, cpha, first_bit);
  } else {
    // Software SPI
    sck = sck_pin;
    copi = copi_pin;
    cipo = cipo_pin;
    gpio_init(sck);
    gpio_init(cipo);
    gpio_set_dir(sck, GPIO_OUT);
    gpio_set_dir(cipo, GPIO_IN);
    if (copi != cipo) {
      gpio_init(copi);
      gpio_set_dir(copi, GPIO_OUT);
    }
  }

  return ERROR_NONE;
}


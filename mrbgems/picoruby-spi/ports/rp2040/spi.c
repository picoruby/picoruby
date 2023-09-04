#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"

#include "../../include/spi.h"


// For bitbang SPI (workaround)
static uint8_t sck = -1;
static uint8_t copi = -1;
static uint8_t cipo = -1;

static int
bitbang_read_blocking(spi_unit_info_t *unit_info, uint8_t *dst, size_t len)
{
  uint8_t temp;
  int bit;
  gpio_set_dir(unit_info->cipo_pin, GPIO_IN);
  for (int i = 0; i < len; i++) {
    temp = 0;
    for (bit = 0; bit < 8; bit++) {
      busy_wait_us_32(1);
      gpio_put(unit_info->sck_pin, 0);
      busy_wait_us_32(1);
      temp <<= 1;
      temp |= gpio_get(unit_info->cipo_pin);
      busy_wait_us_32(20);
      gpio_put(unit_info->sck_pin, 1);
    }
    dst[i] = temp;
  }
  return len;
}

static int
bitbang_write_blocking(spi_unit_info_t *unit_info, uint8_t *src, size_t len)
{
  uint8_t temp = 0;
  int bit;
  gpio_set_dir(unit_info->copi_pin, GPIO_OUT);
  for (int i = 0; i < len; i++) {
    temp = src[i];
    for (bit = 0; bit < 8; bit++) {
      gpio_put(unit_info->sck_pin, 0);
      busy_wait_us_32(1);
      gpio_put(unit_info->copi_pin, (temp & 0x80) != 0);
      temp <<= 1;
      gpio_put(unit_info->sck_pin, 1);
      busy_wait_us_32(1);
    }
  }
  return len;
}

int
SPI_read_blocking(spi_unit_info_t *unit_info, uint8_t *dst, size_t len, uint8_t repeated_tx_data)
{
  spi_inst_t *unit;
  UNIT_SELECT();
  if (PICORUBY_SPI_BITBANG < unit) {
    return spi_read_blocking(unit, repeated_tx_data, dst, len);
  } else {
    return bitbang_read_blocking(unit_info, dst, len);
  }
}

int
SPI_write_blocking(spi_unit_info_t *unit_info, uint8_t *src, size_t len)
{
  spi_inst_t *unit;
  UNIT_SELECT();
  if (PICORUBY_SPI_BITBANG < unit) {
    return spi_write_blocking(unit, src, len);
  } else {
    return bitbang_write_blocking(unit_info, src, len);
  }
}

int
SPI_transfer(spi_unit_info_t *unit_info, uint8_t *txdata, uint8_t *rxdata, size_t len)
{
  spi_inst_t *unit;
  UNIT_SELECT();
  if (PICORUBY_SPI_BITBANG < unit) {
    return spi_write_read_blocking(unit, txdata, rxdata, len);
  } else {
    return ERROR_NOT_IMPLEMENTED;
  }
}

int
SPI_unit_name_to_unit_num(const char *unit_name)
{
  if (strcmp(unit_name, "RP2040_SPI0") == 0) {
    return PICORUBY_SPI_RP2040_SPI0;
  } else if (strcmp(unit_name, "RP2040_SPI1") == 0) {
    return PICORUBY_SPI_RP2040_SPI1;
  } else {
    return ERROR_INVALID_UNIT;
  }
}

spi_status_t
SPI_gpio_init(spi_unit_info_t *unit_info)
{
  spi_inst_t *unit;
  UNIT_SELECT();

  if (unit_info->sck_pin  < 0) { unit_info->sck_pin  = PICO_DEFAULT_SPI_SCK_PIN; }
  if (unit_info->cipo_pin < 0) { unit_info->cipo_pin = PICO_DEFAULT_SPI_RX_PIN; }
  if (unit_info->copi_pin < 0) { unit_info->copi_pin = PICO_DEFAULT_SPI_TX_PIN; }

  if (unit) {
    spi_init(unit, unit_info->frequency);
    gpio_set_function(unit_info->sck_pin, GPIO_FUNC_SPI);
    gpio_set_function(unit_info->cipo_pin, GPIO_FUNC_SPI);
    gpio_set_function(unit_info->copi_pin, GPIO_FUNC_SPI);

    /* mode */
    if (unit_info->first_bit != SPI_MSB_FIRST) {
      /* RP2040 supports only MSB first */
      return ERROR_INVALID_FIRST_BIT;
    }
    spi_cpol_t cpol;
    spi_cpha_t cpha;
    switch (unit_info->mode) {
      case 0: { cpol = 0; cpha = 0; break; }
      case 1: { cpol = 0; cpha = 1; break; }
      case 2: { cpol = 1; cpha = 0; break; }
      case 3: { cpol = 1; cpha = 1; break; }
      default: { return ERROR_INVALID_MODE; }
    }
    spi_set_format(unit, unit_info->data_bits, cpol, cpha, unit_info->first_bit);
  } else {
    // bitbang SPI
    gpio_init(unit_info->sck_pin);
    gpio_put(unit_info->sck_pin, 1);
    gpio_init(unit_info->cipo_pin);
    gpio_set_dir(unit_info->sck_pin, GPIO_OUT);
    gpio_set_dir(unit_info->cipo_pin, GPIO_IN);
    if (unit_info->copi_pin != unit_info->cipo_pin) {
      gpio_init(unit_info->copi_pin);
      gpio_set_dir(unit_info->copi_pin, GPIO_OUT);
    }
  }

  return ERROR_NONE;
}


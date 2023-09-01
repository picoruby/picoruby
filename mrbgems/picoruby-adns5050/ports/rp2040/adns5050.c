#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "../../include/adns5050.h"

static uint8_t sclk;
static uint8_t sdio;
static uint8_t ncs;

void
ADNS5050_init(uint8_t sclk_pin, uint8_t sdio_pin, uint8_t ncs_pin)
{
  sclk = sclk_pin;
  gpio_init(sclk);
  gpio_set_dir(sclk, true);
  gpio_put(sclk, 1);
  sdio = sdio_pin;
  gpio_init(sdio);
  gpio_set_dir(sdio, true);
  gpio_put(sdio, 0);
  ncs = ncs_pin;
  gpio_init(ncs);
  gpio_set_dir(ncs, true);
  gpio_put(ncs, 1);
}

int
ADNS5050_read8(uint8_t addr)
{
  uint8_t temp = 0;
  int n;
  gpio_put(ncs, 0);
  temp = addr;
  gpio_put(sclk, 0); // start clock
  gpio_set_dir(sdio, true); // output
  for (n = 0; n < 8; n++) {
    gpio_put(sclk, 0);
    gpio_put(sdio, temp & 0x80);
    busy_wait_us_32(2);
    temp <<= 1;
    gpio_put(sclk, 1);
  }
  temp = 0;
  gpio_set_dir(sdio, false); // input
  for (n = 0; n < 8; n++) {
    busy_wait_us_32(1);
    gpio_put(sclk, 0);
    busy_wait_us_32(1);
    temp <<= 1;
    temp |= gpio_get(sdio);
    busy_wait_us_32(20);
    gpio_put(sclk, 1);
  }
  gpio_put(ncs, 1);
  return temp;
}

void
ADNS5050_write8(uint8_t addr, uint8_t data)
{
  uint8_t temp = 0;
  int n;
  gpio_put(ncs, 0);
  temp = addr;
  gpio_put(sclk, 0); // start clock
  gpio_set_dir(sdio, true); // output
  for (n = 0; n < 8; n++) {
    gpio_put(sclk, 0);
    busy_wait_us_32(1);
    gpio_put(sdio, temp & 0x80);
    temp <<= 1;
    gpio_put(sclk, 1);
    busy_wait_us_32(1);
  }
  temp = data;
  for (n = 0; n < 8; n++) {
    gpio_put(sclk, 0);
    busy_wait_us_32(1);
    gpio_put(sdio, temp & 0x80);
    temp <<= 1;
    gpio_put(sclk, 1);
    busy_wait_us_32(1);
  }
}

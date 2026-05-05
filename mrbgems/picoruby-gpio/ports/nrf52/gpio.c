#include <stdint.h>

#include "nrf_gpio.h"

#include "../../include/gpio.h"

int
GPIO_pin_num_from_char(const uint8_t *str)
{
  (void)str;
  return -1;
}

void
GPIO_init(uint8_t pin)
{
  nrf_gpio_cfg_default(pin);
}

void
GPIO_set_dir(uint8_t pin, uint8_t dir)
{
  switch (dir) {
    case IN:
      nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_NOPULL);
      break;
    case OUT:
      nrf_gpio_cfg_output(pin);
      break;
    case HIGH_Z:
      nrf_gpio_cfg(pin,
        NRF_GPIO_PIN_DIR_INPUT,
        NRF_GPIO_PIN_INPUT_DISCONNECT,
        NRF_GPIO_PIN_NOPULL,
        NRF_GPIO_PIN_S0S1,
        NRF_GPIO_PIN_NOSENSE);
      break;
  }
}

void
GPIO_open_drain(uint8_t pin)
{
  (void)pin;
}

void
GPIO_pull_up(uint8_t pin)
{
  nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLUP);
}

void
GPIO_pull_down(uint8_t pin)
{
  nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLDOWN);
}

int
GPIO_read(uint8_t pin)
{
  return nrf_gpio_pin_read(pin);
}

void
GPIO_write(uint8_t pin, uint8_t val)
{
  nrf_gpio_pin_write(pin, val ? 1u : 0u);
}

void
GPIO_set_function(uint8_t pin, uint8_t function)
{
  (void)pin;
  (void)function;
}

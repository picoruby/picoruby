#include <stdint.h>
#include <stdbool.h>
#include "hardware/gpio.h"

#include "../../include/gpio.h"

int
GPIO_pin_num_from_char(const uint8_t *str)
{
  /* Not supported */
  return -1;
}

void
GPIO_init(uint8_t pin)
{
  gpio_init(pin);
}

void
GPIO_set_dir(uint8_t pin, uint8_t dir)
{
  switch (dir) {
    case (IN):
      gpio_set_dir(pin, false);
      break;
    case (OUT):
      gpio_set_dir(pin, true);
      break;
    case (HIGH_Z):
      /* HIGH_Z is not supported */
      break;
  }
}

void
GPIO_open_drain(uint8_t pin)
{
  /* Not supported */
}

void
GPIO_pull_up(uint8_t pin)
{
  gpio_pull_up(pin);
}

void
GPIO_pull_down(uint8_t pin)
{
  gpio_pull_down(pin);
}

int
GPIO_read(uint8_t pin)
{
  return gpio_get(pin);
}

void
GPIO_write(uint8_t pin, uint8_t val)
{
  gpio_put(pin, val == 1);
}


#include <stdint.h>
#include "driver/gpio.h"

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
  gpio_reset_pin(pin);
}

void
GPIO_set_dir(uint8_t pin, uint8_t dir)
{
  switch (dir) {
    case (IN):
      gpio_set_direction(pin, GPIO_MODE_INPUT);
      break;
    case (OUT):
      gpio_set_direction(pin, GPIO_MODE_OUTPUT);
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
  gpio_pullup_en(pin);
}

void
GPIO_pull_down(uint8_t pin)
{
  gpio_pulldown_en(pin);
}

int
GPIO_read(uint8_t pin)
{
  return gpio_get_level(pin);
}

void
GPIO_write(uint8_t pin, uint8_t val)
{
  gpio_set_level(pin, val);
}

void
GPIO_set_function(uint8_t pin, uint8_t function)
{
  /* Not supported */
}

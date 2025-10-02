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
}

void
GPIO_set_dir(uint8_t pin, uint8_t dir)
{
}

void
GPIO_open_drain(uint8_t pin)
{
  /* Not supported */
}

void
GPIO_pull_up(uint8_t pin)
{
}

void
GPIO_pull_down(uint8_t pin)
{
}

int
GPIO_read(uint8_t pin)
{
  return 1;
}

void
GPIO_write(uint8_t pin, uint8_t val)
{
}

void
GPIO_set_function(uint8_t pin, uint8_t function)
{
}

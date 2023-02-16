#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "hardware/adc.h"

#include "../../include/adc.h"

#define TEMPERATURE 255

int
ADC_pin_num_from_char(const uint8_t *str)
{
  if (strcmp(str, "temperature") == 0) {
    return TEMPERATURE;
  } else {
    return -1;
  }
}

int
ADC_init(uint8_t pin)
{
  uint input;
  switch (pin) {
    case 26: { input = 0; break; }
    case 27: { input = 1; break; }
    case 28: { input = 2; break; }
    case 29: { input = 3; break; }
    case TEMPERATURE: { input = 4; break; }
    default: { return -1; }
  }
  adc_init();
  if (pin != TEMPERATURE) {
    adc_gpio_init(input);
  }
  return (int)input;
}

uint16_t
ADC_read(uint8_t input)
{
  adc_select_input(input);
  return adc_read();
}


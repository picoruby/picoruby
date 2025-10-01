#include <stdint.h>

#include "../../include/adc.h"

int
ADC_pin_num_from_char(const uint8_t *str)
{
  (void)str;
  return 0;
}

int
ADC_init(uint8_t pin)
{
  return pin;
}

uint32_t
ADC_read_raw(uint8_t input)
{
  return 0;
}

#ifndef PICORB_NO_FLOAT
picorb_float_t
ADC_read_voltage(uint8_t input)
{
  return (picorb_float_t)0.0;
}
#endif

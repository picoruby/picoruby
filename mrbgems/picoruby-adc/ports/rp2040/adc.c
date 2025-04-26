#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "hardware/adc.h"

#include "../../include/adc.h"

#define VOLTAGE_MAX 3.3
#define RESOLUTION 4095
#define TEMPERATURE 255

int
ADC_pin_num_from_char(const uint8_t *str)
{
  if (strcmp((const char *)str, "temperature") == 0) {
    return TEMPERATURE;
  } else {
    return -1;
  }
}

int
ADC_init(uint8_t pin)
{
  static bool init = false;
  if (!init) {
    adc_init();
    init = true;
  }
  uint input;
  switch (pin) {
    case 26: { input = 0; break; }
    case 27: { input = 1; break; }
    case 28: { input = 2; break; }
    case 29: { input = 3; break; }
    case TEMPERATURE: { input = 4; break; }
    default: { return -1; }
  }
  if (pin == TEMPERATURE) {
    adc_set_temp_sensor_enabled(true);
  } else {
    adc_gpio_init(input);
  }
  return (int)input;
}

uint32_t
ADC_read_raw(uint8_t input)
{
  adc_select_input(input);
  return (uint32_t)adc_read();
}

#ifndef PICORB_NO_FLOAT
picorb_float_t
ADC_read_voltage(uint8_t input)
{
  adc_select_input(input);
  return (picorb_float_t)adc_read() * VOLTAGE_MAX / RESOLUTION;
}
#endif

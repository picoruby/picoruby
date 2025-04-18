#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "../../include/adc.h"

#define VOLTAGE_MAX 3.3
#define RESOLUTION 4095
#define UNIT_NUM 2

static adc_oneshot_unit_handle_t adc_handles[UNIT_NUM];

static adc_unit_t
pin_to_unit(uint8_t pin)
{
  switch (pin) {
#if CONFIG_IDF_TARGET_ESP32C3
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
      return ADC_UNIT_1;
    case 5:
      return ADC_UNIT_2;
#elif CONFIG_IDF_TARGET_ESP32
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 39:
      return ADC_UNIT_1;
    case 0:
    case 2:
    case 4:
    case 12:
    case 13:
    case 14:
    case 15:
    case 25:
    case 26:
    case 27:
      return ADC_UNIT_2;
#endif
  }
  return -1;
}

static adc_oneshot_unit_handle_t
pin_to_unit_handle(uint8_t pin)
{
  adc_unit_t unit = pin_to_unit(pin);
  if ((unit < 0) || (unit >= UNIT_NUM)) {
    return NULL;
  }
  return adc_handles[unit];
}

static adc_channel_t
pin_to_channel(uint8_t pin)
{
  switch (pin) {
#if CONFIG_IDF_TARGET_ESP32C3
    case 0: return ADC_CHANNEL_0;
    case 1: return ADC_CHANNEL_1;
    case 2: return ADC_CHANNEL_2;
    case 3: return ADC_CHANNEL_3;
    case 4: return ADC_CHANNEL_4;
    case 5: return ADC_CHANNEL_0;
#elif CONFIG_IDF_TARGET_ESP32
    case 36: return ADC_CHANNEL_0;
    case 39: return ADC_CHANNEL_3;
    case 32: return ADC_CHANNEL_4;
    case 33: return ADC_CHANNEL_5;
    case 34: return ADC_CHANNEL_6;
    case 35: return ADC_CHANNEL_7;
    case 4:  return ADC_CHANNEL_0;
    case 0:  return ADC_CHANNEL_1;
    case 2:  return ADC_CHANNEL_2;
    case 15: return ADC_CHANNEL_3;
    case 13: return ADC_CHANNEL_4;
    case 12: return ADC_CHANNEL_5;
    case 14: return ADC_CHANNEL_6;
    case 27: return ADC_CHANNEL_7;
    case 25: return ADC_CHANNEL_8;
    case 26: return ADC_CHANNEL_9;
#endif
  }
  return -1;
}

static int
init_adc(void)
{
  adc_unit_t units[UNIT_NUM] = { ADC_UNIT_1, ADC_UNIT_2 };
  
  for (int i = 0; i < UNIT_NUM ; i++) {
    adc_oneshot_unit_init_cfg_t init_config = {
      .unit_id = units[i],
      .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    if (adc_oneshot_new_unit(&init_config, &adc_handles[i]) != ESP_OK) {
      return -1;
    }
  }

  return 0;
}

int
ADC_pin_num_from_char(const uint8_t *str)
{
  return -1;
}

int
ADC_init(uint8_t pin)
{
  static bool init = false;
  if (!init) {
    if (init_adc() != 0) {
      return -1;
    }
    init = true;
  }

  int channel = pin_to_channel(pin);
  if (channel < 0) {
    return -1;
  }

  adc_oneshot_chan_cfg_t config = {
    .atten = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_DEFAULT,
  };

  adc_oneshot_unit_handle_t handle = pin_to_unit_handle(pin);
  if (adc_oneshot_config_channel(handle, channel, &config) != ESP_OK) {
    return -1;
  }

  return (int)pin;
}

uint32_t
ADC_read_raw(uint8_t input)
{
  adc_oneshot_unit_handle_t handle = pin_to_unit_handle(input);
  uint8_t channel = pin_to_channel(input);
  
  int raw = 0;
  if (adc_oneshot_read(handle, channel, &raw) != ESP_OK) {
    return 0;
  }
  return (uint32_t)raw;
}

#if MRBC_USE_FLOAT
picobc_float_t
ADC_read_voltage(uint8_t input)
{
  uint32_t raw = ADC_read_raw(input);
  return raw * VOLTAGE_MAX / RESOLUTION;
}
#endif

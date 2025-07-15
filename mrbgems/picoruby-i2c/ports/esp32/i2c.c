#include <string.h>
#include "driver/i2c_master.h"

#include "../../include/i2c.h"

// Structure to store the I2C bus handle
typedef struct {
  i2c_master_bus_handle_t bus_handle;
  bool initialized;
  uint32_t frequency;
} i2c_bus_context_t;

// Context for each I2C port (ESP32 has a maximum of 2 ports)
static i2c_bus_context_t i2c_contexts[2] = {0};

int
I2C_read_timeout_us(int unit_num, uint8_t addr, uint8_t* dst, size_t len, bool nostop, uint32_t timeout_us)
{
  if (!i2c_contexts[unit_num].initialized) {
    return I2C_ERROR_INVALID_UNIT;
  }

  i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = addr,
    .scl_speed_hz = i2c_contexts[unit_num].frequency,
  };

  i2c_master_dev_handle_t dev_handle;
  esp_err_t err = i2c_master_bus_add_device(i2c_contexts[unit_num].bus_handle, &dev_cfg, &dev_handle);
  if (err != ESP_OK) {
    return -1;
  }

  // Convert timeout to milliseconds (minimum 10ms)
  uint32_t timeout_ms = (timeout_us + 999) / 1000;
  if (timeout_ms < 10) {
    timeout_ms = 10;
  }

  err = i2c_master_receive(dev_handle, dst, len, timeout_ms);

  i2c_master_bus_rm_device(dev_handle);

  if (err != ESP_OK) {
    return -1;
  }

  return len;
}

int
I2C_write_timeout_us(int unit_num, uint8_t addr, uint8_t* src, size_t len, bool nostop, uint32_t timeout_us)
{
  if (!i2c_contexts[unit_num].initialized) {
    return I2C_ERROR_INVALID_UNIT;
  }

  i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = addr,
    .scl_speed_hz = i2c_contexts[unit_num].frequency,
  };

  i2c_master_dev_handle_t dev_handle;
  esp_err_t err = i2c_master_bus_add_device(i2c_contexts[unit_num].bus_handle, &dev_cfg, &dev_handle);
  if (err != ESP_OK) {
    return -1;
  }

  // Convert timeout to milliseconds (minimum 10ms)
  uint32_t timeout_ms = (timeout_us + 999) / 1000;
  if (timeout_ms < 10) {
    timeout_ms = 10;
  }

  err = i2c_master_transmit(dev_handle, src, len, timeout_ms);

  i2c_master_bus_rm_device(dev_handle);

  if (err != ESP_OK) {
    return -1;
  }

  return len;
}

int
I2C_unit_name_to_unit_num(const char *unit_name)
{
  if (strcmp(unit_name, "ESP32_I2C0") == 0) {
    return 0;
  } else if (strcmp(unit_name, "ESP32_I2C1") == 0) {
    return 1;
  } else {
    return I2C_ERROR_INVALID_UNIT;
  }
}

i2c_status_t
I2C_gpio_init(int unit_num, uint32_t frequency, int8_t sda_pin, int8_t scl_pin)
{
  if (i2c_contexts[unit_num].initialized) {
    i2c_del_master_bus(i2c_contexts[unit_num].bus_handle);
    i2c_contexts[unit_num].initialized = false;
  }

  i2c_master_bus_config_t i2c_mst_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = unit_num,
    .scl_io_num = scl_pin,
    .sda_io_num = sda_pin,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
  };

  esp_err_t err = i2c_new_master_bus(&i2c_mst_config, &i2c_contexts[unit_num].bus_handle);
  if (err != ESP_OK) {
    return I2C_ERROR_INVALID_UNIT;
  }

  i2c_contexts[unit_num].initialized = true;
  i2c_contexts[unit_num].frequency = frequency;

  return I2C_ERROR_NONE;
}

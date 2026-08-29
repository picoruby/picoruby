#include <string.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_rom_gpio.h"
#include "soc/i2c_periph.h"

#include "../../include/i2c.h"

// Structure to store the I2C bus handle
typedef struct {
  i2c_master_bus_handle_t bus_handle;
  bool initialized;
  uint32_t frequency;
  int8_t sda_pin;
  int8_t scl_pin;
} i2c_bus_context_t;

// Context for each I2C port (ESP32 has a maximum of 2 ports)
static i2c_bus_context_t i2c_contexts[2] = {0};

// Re-assert this unit's pad routing before every transfer.
//
// The pins are configured once, in i2c_new_master_bus(), and nothing touches
// them again for the lifetime of the bus. That is fine while this port is the
// only thing driving them. It stops being fine as soon as another driver
// talks to another device over the same two pins -- an I2C touch screen owned
// by a display library is the usual case, since a board that exposes an I2C
// connector often hangs it off the touch controller's bus. Such a driver
// points the pins at its own I2C unit, typically on every transaction, and
// leaves them there.
//
// Only one side can win: an ESP32 pad carries the output signal of a single
// peripheral at a time and the routing is last-writer-wins, so once the other
// driver has run we drive neither SDA nor SCL and every transfer fails. It
// does not recover on its own either -- on targets with
// SOC_I2C_SUPPORT_HW_FSM_RST the driver's error path resets the state machine
// without re-running the pin configuration.
//
// Re-asserting the routing costs a handful of register writes against a
// transfer measured in hundreds of microseconds, and is a no-op when nothing
// else touches the pins.
static void
i2c_reclaim_pins(int unit_num)
{
  int8_t sda_pin = i2c_contexts[unit_num].sda_pin;
  int8_t scl_pin = i2c_contexts[unit_num].scl_pin;

  if (sda_pin < 0 || scl_pin < 0) {
    return;
  }

  gpio_set_level((gpio_num_t)sda_pin, 1);
  gpio_set_direction((gpio_num_t)sda_pin, GPIO_MODE_INPUT_OUTPUT_OD);
  gpio_set_pull_mode((gpio_num_t)sda_pin, GPIO_PULLUP_ONLY);
  esp_rom_gpio_connect_out_signal(sda_pin, i2c_periph_signal[unit_num].sda_out_sig, false, false);
  esp_rom_gpio_connect_in_signal(sda_pin, i2c_periph_signal[unit_num].sda_in_sig, false);

  gpio_set_level((gpio_num_t)scl_pin, 1);
  gpio_set_direction((gpio_num_t)scl_pin, GPIO_MODE_INPUT_OUTPUT_OD);
  gpio_set_pull_mode((gpio_num_t)scl_pin, GPIO_PULLUP_ONLY);
  esp_rom_gpio_connect_out_signal(scl_pin, i2c_periph_signal[unit_num].scl_out_sig, false, false);
  esp_rom_gpio_connect_in_signal(scl_pin, i2c_periph_signal[unit_num].scl_in_sig, false);
}

int
I2C_read_timeout_us(int unit_num, uint8_t addr, uint8_t* dst, size_t len, bool nostop, uint32_t timeout_us)
{
  if (!i2c_contexts[unit_num].initialized) {
    return I2C_ERROR_INVALID_UNIT;
  }

  i2c_reclaim_pins(unit_num);

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

  i2c_reclaim_pins(unit_num);

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
  i2c_contexts[unit_num].sda_pin = sda_pin;
  i2c_contexts[unit_num].scl_pin = scl_pin;

  return I2C_ERROR_NONE;
}

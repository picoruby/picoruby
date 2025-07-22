#include <stdio.h>
#include <string.h>

#include "driver/spi_master.h"

#include "../../include/spi.h"

static spi_device_handle_t spi_handles[SPI_HOST_MAX] = { NULL };

int
SPI_read_blocking(spi_unit_info_t *unit_info, uint8_t *dst, size_t len, uint8_t repeated_tx_data)
{
  spi_transaction_t t = {
    .length = len * 8,
    .tx_buffer = NULL,
    .rx_buffer = dst,
    .user = NULL
  };
  
  esp_err_t ret = spi_device_polling_transmit(spi_handles[unit_info->unit_num], &t);
  return (ret == ESP_OK) ? len : -1;
}

int
SPI_write_blocking(spi_unit_info_t *unit_info, uint8_t *src, size_t len)
{
  spi_transaction_t t = {
    .length = len * 8,
    .tx_buffer = src,
    .rx_buffer = NULL,
    .user = NULL
  };

  esp_err_t ret = spi_device_polling_transmit(spi_handles[unit_info->unit_num], &t);
  return (ret == ESP_OK) ? len : -1;
}

int
SPI_transfer(spi_unit_info_t *unit_info, uint8_t *txdata, uint8_t *rxdata, size_t len)
{
  spi_transaction_t t = {
    .length = len * 8,
    .tx_buffer = txdata,
    .rx_buffer = rxdata,
    .user = NULL
  };

  esp_err_t ret = spi_device_polling_transmit(spi_handles[unit_info->unit_num], &t);
  return (ret == ESP_OK) ? len : -1;
}

int
SPI_unit_name_to_unit_num(const char *unit_name)
{
  if (strcmp(unit_name, "ESP32_SPI2_HOST") == 0) {
    return SPI2_HOST;
  } if (strcmp(unit_name, "ESP32_HSPI_HOST") == 0) {
    return SPI2_HOST;
#if (SOC_SPI_PERIPH_NUM == 3)
  } else if (strcmp(unit_name, "ESP32_SPI3_HOST") == 0) {
    return SPI3_HOST;
  } else if (strcmp(unit_name, "ESP32_VSPI_HOST") == 0) {
    return SPI3_HOST;
#endif
  } else {
    return SPI_ERROR_INVALID_UNIT;
  }
}

spi_status_t
SPI_gpio_init(spi_unit_info_t *unit_info)
{
  if (spi_handles[unit_info->unit_num] != NULL) {
    return SPI_ERROR_NONE;
  }

  spi_bus_config_t buscfg = {
    .mosi_io_num = unit_info->copi_pin,
    .miso_io_num = unit_info->cipo_pin,
    .sclk_io_num = unit_info->sck_pin,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1
  };

  esp_err_t ret = spi_bus_initialize(unit_info->unit_num, &buscfg, SPI_DMA_CH_AUTO);
  if (ret != ESP_OK) {
    return SPI_ERROR_FAILED_TO_INIT;
  }

  spi_device_interface_config_t devcfg = {
    .clock_speed_hz = unit_info->frequency,
    .mode = unit_info->mode,
    .spics_io_num = unit_info->cs_pin,
    .queue_size = 7
  };

  ret = spi_bus_add_device(unit_info->unit_num, &devcfg, &spi_handles[unit_info->unit_num]);
  if (ret != ESP_OK) {
    spi_bus_free(unit_info->unit_num);
    spi_handles[unit_info->unit_num] = NULL;
    return SPI_ERROR_FAILED_TO_ADD_DEV;
  }
  return SPI_ERROR_NONE;
}

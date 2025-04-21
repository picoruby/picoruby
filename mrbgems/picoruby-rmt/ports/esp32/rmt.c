#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "../../include/rmt.h"

#define RMT_RESOLUTION_HZ (10000000)
#define RMT_MEM_BLOCK_SYMBOLS (64)
#define RMT_TRANS_QUEUE_DEPTH (4)

static rmt_channel_handle_t rmt_channel = NULL;
static rmt_encoder_handle_t rmt_encoder = NULL;
static RMT_symbol_dulation_t rmt_symbol_dulation;

static size_t
encoder_callback(const void *data, size_t data_size,
  size_t symbols_written, size_t symbols_free, rmt_symbol_word_t *symbols, bool *done, void *arg)
{
  if (symbols_free < 8) {
    return 0;
  }

  const rmt_symbol_word_t symbol_zero = {
    .level0 = 1,
    .duration0 = (uint16_t)((float)rmt_symbol_dulation.t0h_ns * RMT_RESOLUTION_HZ / 1000000000),
    .level1 = 0,
    .duration1 = (uint16_t)((float)rmt_symbol_dulation.t0l_ns * RMT_RESOLUTION_HZ / 1000000000),
  };
  const rmt_symbol_word_t symbol_one = {
    .level0 = 1,
    .duration0 = (uint16_t)((float)rmt_symbol_dulation.t1h_ns * RMT_RESOLUTION_HZ / 1000000000),
    .level1 = 0,
    .duration1 = (uint16_t)((float)rmt_symbol_dulation.t1l_ns * RMT_RESOLUTION_HZ / 1000000000),
  };
  const rmt_symbol_word_t symbol_reset = {
    .level0 = 0,
    .duration0 = (uint16_t)((float)rmt_symbol_dulation.reset_ns * RMT_RESOLUTION_HZ / 1000000000),
    .level1 = 0,
    .duration1 = (uint16_t)((float)rmt_symbol_dulation.reset_ns * RMT_RESOLUTION_HZ / 1000000000),
  };

  size_t data_pos = symbols_written / 8;
  uint8_t *data_bytes = (uint8_t*)data;
  if (data_pos < data_size) {
    size_t symbol_pos = 0;
    for (int bitmask = 0x80; bitmask != 0; bitmask >>= 1) {
      if (data_bytes[data_pos]&bitmask) {
        symbols[symbol_pos++] = symbol_one;
      } else {
        symbols[symbol_pos++] = symbol_zero;
      }
    }
    return symbol_pos;
  } else {
    symbols[0] = symbol_reset;
    *done = 1;
    return 1;
  }
}

int
RMT_init(uint32_t gpio, RMT_symbol_dulation_t *rsd)
{
  if (gpio >= GPIO_NUM_MAX) return -1;

  rmt_symbol_dulation = *rsd;

  rmt_tx_channel_config_t tx_chan_config = {
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .gpio_num = gpio,
    .mem_block_symbols = RMT_MEM_BLOCK_SYMBOLS,
    .resolution_hz = RMT_RESOLUTION_HZ,
    .trans_queue_depth = RMT_TRANS_QUEUE_DEPTH,
  };
  esp_err_t ret = rmt_new_tx_channel(&tx_chan_config, &rmt_channel);
  if(ret != ESP_OK) {
    return -1;
  }

  const rmt_simple_encoder_config_t simple_encoder_cfg = {
    .callback = encoder_callback
  };
  ret = rmt_new_simple_encoder(&simple_encoder_cfg, &rmt_encoder);
  if(ret != ESP_OK) {
    return -1;
  }

  ret = rmt_enable(rmt_channel);
  if(ret != ESP_OK) {
    return -1;
  }

  return 0;
}

int
RMT_write(uint8_t *buffer, uint32_t nbytes)
{
  rmt_transmit_config_t tx_config = {
    .loop_count = 0,
  };
  esp_err_t ret = rmt_transmit(rmt_channel, rmt_encoder, buffer, nbytes, &tx_config);
  if(ret != ESP_OK) {
    return -1;
  }

  ret = rmt_tx_wait_all_done(rmt_channel, portMAX_DELAY);
  if(ret != ESP_OK) {
    return -1;
  }

  return 0;
}

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

#ifndef PSG_MCP4922_DMA
#define PSG_MCP4922_DMA 0
#endif

#if PSG_MCP4922_DMA
#include "hardware/dma.h"
#endif

#include "../../include/psg.h"


#include "psg_drv_mcp4922_pio.pio.h"

#define DAC_SPI_BAUD  16000000   /* 16 MHz */
#define MCP4922_PIO_CYCLES_PER_SAMPLE 151.0f
#define MCP4922_DMA_SAMPLES (BUF_SAMPLES / 2)

// MCP4922 DAC configuration structure
typedef struct {
  PIO pio;
  uint sm;
  uint copi_pin;
  uint sck_pin;
  uint cs_pin;
  uint ldac_pin;
  uint offset;
  bool initialized;
} mcp4922_pio_config_t;

static mcp4922_pio_config_t dac_config;

#if PSG_MCP4922_DMA
static int dma_chan = -1;
static dma_channel_config dma_config;
static uint32_t dma_buf[MCP4922_DMA_SAMPLES];
#endif

// Initialize MCP4922 PIO DAC
static inline void
mcp4922_pio_init(mcp4922_pio_config_t *config, PIO pio, uint sm, uint ldac_pin, float clk_div)
{
  uint cs_pin   = ldac_pin + 1;
  uint sck_pin  = ldac_pin + 2;
  uint copi_pin = ldac_pin + 3;

  config->pio       = pio;
  config->sm        = sm;
  config->ldac_pin  = ldac_pin;
  config->cs_pin    = cs_pin;
  config->sck_pin   = sck_pin;
  config->copi_pin  = copi_pin;

  config->offset = pio_add_program(pio, &mcp4922_dual_program);

  pio_sm_config c = mcp4922_dual_program_get_default_config(config->offset);

  sm_config_set_set_pins(&c, ldac_pin, 2);         // LDAC, CS
  sm_config_set_out_pins(&c, copi_pin, 1);         // COPI
  sm_config_set_sideset_pins(&c, sck_pin);         // SCK

  sm_config_set_out_shift(&c, false, true, 32);
  sm_config_set_clkdiv(&c, clk_div);

  // GPIO Init
  for (int i = 0; i < 4; ++i)
    pio_gpio_init(pio, ldac_pin + i);

  pio_sm_set_consecutive_pindirs(pio, sm, ldac_pin, 4, true);

  pio_sm_init(pio, sm, config->offset, &c);
  pio_sm_set_enabled(pio, sm, true);

  config->initialized = true;
}

// Write to MCP4922 DAC (blocking version)
static inline void
mcp4922_pio_write_blocking(mcp4922_pio_config_t *config, uint16_t left, uint16_t right)
{
  if (!config->initialized) return;

  uint16_t chA = 0x3000 | (left & 0x0FFF);
  uint16_t chB = 0xB000 | (right & 0x0FFF);
  uint32_t data = ((uint32_t)chA << 16) | chB;

  pio_sm_put_blocking(config->pio, config->sm, data);
}



static void
psg_mcp4922_init(uint8_t ldac, uint8_t _v)
{
  (void)_v;
#if PSG_MCP4922_DMA
  float clk_div = (float)clock_get_hz(clk_sys) / (SAMPLE_RATE * MCP4922_PIO_CYCLES_PER_SAMPLE);
#else
  float clk_div = (float)clock_get_hz(clk_sys) / (DAC_SPI_BAUD * 4.0f);
#endif
  mcp4922_pio_init(&dac_config, pio0, 0, ldac, clk_div);

#if PSG_MCP4922_DMA
  dma_chan = dma_claim_unused_channel(true);
  dma_config = dma_channel_get_default_config(dma_chan);
  channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
  channel_config_set_read_increment(&dma_config, true);
  channel_config_set_write_increment(&dma_config, false);
  channel_config_set_dreq(&dma_config, pio_get_dreq(dac_config.pio, dac_config.sm, true));
  dma_channel_configure(
    dma_chan,
    &dma_config,
    &dac_config.pio->txf[dac_config.sm],
    NULL,
    0,
    false
  );
#endif
}

void
psg_mcp4922_start(void)
{
}

void
psg_mcp4922_stop(void)
{
#if PSG_MCP4922_DMA
  if (dma_chan >= 0) {
    dma_channel_abort(dma_chan);
    dma_channel_cleanup(dma_chan);
    dma_channel_unclaim(dma_chan);
    dma_chan = -1;
  }
#endif

  if (dac_config.initialized) {
    // Disable the state machine
    pio_sm_set_enabled(dac_config.pio, dac_config.sm, false);
    // Remove the program from PIO memory
    pio_remove_program(dac_config.pio, &mcp4922_dual_program, dac_config.offset);
    // Reset the initialized flag
    dac_config.initialized = false;
  }
}

static inline void
psg_mcp4922_write(uint16_t l, uint16_t r)
{
  mcp4922_pio_write_blocking(&dac_config, l, r);
}

#if PSG_MCP4922_DMA
static bool
psg_mcp4922_write_buffer(const uint32_t *samples, uint32_t count)
{
  if (!dac_config.initialized || dma_chan < 0 || count > MCP4922_DMA_SAMPLES) return false;
  if (dma_channel_is_busy(dma_chan)) return false;

  for (uint32_t i = 0; i < count; i++) {
    uint16_t left = samples[i] >> 16;
    uint16_t right = samples[i] & 0x0FFF;
    uint16_t chA = 0x3000 | (left & 0x0FFF);
    uint16_t chB = 0xB000 | right;
    dma_buf[i] = ((uint32_t)chA << 16) | chB;
  }

  dma_channel_configure(
    dma_chan,
    &dma_config,
    &dac_config.pio->txf[dac_config.sm],
    dma_buf,
    count,
    true
  );
  return true;
}
#endif

const psg_output_api_t psg_drv_mcp4922 = {
  .init         = psg_mcp4922_init,
  .start        = psg_mcp4922_start,
  .stop         = psg_mcp4922_stop,
  .write        = psg_mcp4922_write,
#if PSG_MCP4922_DMA
  .write_buffer = psg_mcp4922_write_buffer
#else
  .write_buffer = NULL
#endif
};

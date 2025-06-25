#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

#include "../../include/psg.h"
#include "hardware/dma.h"

#include "psg_drv_mcp4922_pio.pio.h"

#define DAC_SPI_BAUD  16000000   /* 16 MHz */

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

// Non-blocking version with DMA support preparation
static inline bool
mcp4922_pio_write_ready(mcp4922_pio_config_t *config)
{
  return !pio_sm_is_tx_fifo_full(config->pio, config->sm);
}

static inline void
mcp4922_pio_write_nonblocking(mcp4922_pio_config_t *config, uint16_t data)
{
  pio_sm_put(config->pio, config->sm, data);
}

static void
psg_mcp4922_init(uint8_t ldac, uint8_t _v)
{
  (void)_v;
  float clk_div = (float)clock_get_hz(clk_sys) / (DAC_SPI_BAUD * 4.0f);
  mcp4922_pio_init(&dac_config, pio0, 0, ldac, clk_div);
}

void
psg_mcp4922_start(void)
{
}

void
psg_mcp4922_stop(void)
{
}

static inline void
psg_mcp4922_write(uint16_t l, uint16_t r)
{
  mcp4922_pio_write_blocking(&dac_config, l, r);
}

const psg_output_api_t psg_drv_mcp4922 = {
  .init  = psg_mcp4922_init,
  .start = psg_mcp4922_start,
  .stop  = psg_mcp4922_stop,
  .write = psg_mcp4922_write
};


#include "pico/stdlib.h"
#include "hardware/spi.h"

#include "../../include/psg.h"
#include "hardware/dma.h"

#define DAC_SPI0      spi0
#define DAC_SPI1      spi1
#define DAC_SPI_BAUD  10000000   /* 10 MHz */

/* 2-sample DMA buffer */
static uint16_t dac_buf[2];
static int      dac_dma_ch;

static uint ldac_pin;

static void
psg_mcp492x_init(uint8_t mosi, uint8_t sck, uint8_t cs, uint8_t ldac)
{
  spi_inst_t *spi_unit;
  switch (mosi) {
    case 3:
    case 7:
    case 19:
    case 23:
      spi_unit = DAC_SPI0;
      break;
    case 11:
    case 15:
    case 27:
      spi_unit = DAC_SPI1;
      break;
    default:
      return;
  }
  spi_init(spi_unit, DAC_SPI_BAUD);
  gpio_set_function(mosi, GPIO_FUNC_SPI);
  gpio_set_function(sck,  GPIO_FUNC_SPI);
  gpio_init(cs);
  gpio_set_dir(cs, GPIO_OUT);
  gpio_put(cs, 1);
  gpio_init(ldac);
  gpio_set_dir(ldac, GPIO_OUT);
  ldac_pin = ldac;

  dac_dma_ch = dma_claim_unused_channel(true);
  dma_channel_config c = dma_channel_get_default_config(dac_dma_ch);
  channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
  channel_config_set_dreq(&c, spi_get_dreq(spi_unit, true));
  channel_config_set_read_increment(&c, true);
  channel_config_set_write_increment(&c, false);
  dma_channel_configure(dac_dma_ch, &c, &spi_get_hw(spi_unit)->dr, dac_buf, 2, false);
}

void
psg_mcp492x_start(void)
{
  gpio_put(ldac_pin, 0);
}

void
psg_mcp492x_stop(void)
{
  gpio_put(ldac_pin, 1);
}

static inline void
psg_mcp4922_write(uint16_t l, uint16_t r)
{
  while (dma_channel_is_busy(dac_dma_ch));
  // build 16-bit frame: |C1|BUF|GA|SHDN|D11..0|
  dac_buf[0] = 0x3000 | (l & 0x0FFF);  // Channel A
  dac_buf[1] = 0xB000 | (r & 0x0FFF);  // Channel B
  dma_channel_set_read_addr(dac_dma_ch, dac_buf, true);
}

static inline void
psg_mcp4921_write(uint16_t l, uint16_t r)
{
  while (dma_channel_is_busy(dac_dma_ch));
  uint16_t mono = (l + r) / 2;
  dac_buf[0] = 0x3000 | (mono & 0x0FFF);  // Channel A
  dma_channel_set_read_addr(dac_dma_ch, dac_buf, true);
}

const psg_output_api_t psg_drv_mcp4922 = {
  .init  = psg_mcp492x_init,
  .start = psg_mcp492x_start,
  .stop  = psg_mcp492x_stop,
  .write = psg_mcp4922_write
};

const psg_output_api_t psg_drv_mcp4921 = {
  .init  = psg_mcp492x_init,
  .start = psg_mcp492x_start,
  .stop  = psg_mcp492x_stop,
  .write = psg_mcp4921_write // mono
};

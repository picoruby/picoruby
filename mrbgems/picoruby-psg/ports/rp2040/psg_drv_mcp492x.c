#include "pico/stdlib.h"
#include "hardware/spi.h"

#include "../../include/psg.h"
#include "hardware/dma.h"

#define DAC_SPI0      spi0
#define DAC_SPI1      spi1
#define DAC_SPI_BAUD  10000000   /* 10 MHz */

static spi_inst_t *spi_unit;
static uint ldac_pin;
static uint cs_pin;

static void
psg_mcp492x_init(uint8_t copi, uint8_t sck, uint8_t cs, uint8_t ldac)
{
  ldac_pin = ldac;
  cs_pin = cs;
  switch (copi) {
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
  gpio_set_function(copi, GPIO_FUNC_SPI);
  gpio_set_function(sck,  GPIO_FUNC_SPI);
  spi_set_format(spi_unit, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
  gpio_init(cs_pin);
  gpio_set_dir(cs_pin, GPIO_OUT);
  gpio_put(cs_pin, 1);
  gpio_init(ldac_pin);
  gpio_set_dir(ldac_pin, GPIO_OUT);
  gpio_put(ldac_pin, 0);
}

void
psg_mcp492x_start(void)
{
}

void
psg_mcp492x_stop(void)
{
}

static inline void
psg_mcp4922_write(uint16_t l, uint16_t r)
{
  uint16_t data_l = 0x3000 | (l & 0x0FFF);  // Channel A
  uint16_t data_r = 0xB000 | (r & 0x0FFF);  // Channel B
  gpio_put(ldac_pin, 1); // set LDAC high to prepare for transfer
  gpio_put(cs_pin, 0);   // set CS low to start transfer
  spi_write16_blocking(spi_unit, &data_l, 1); // write left channel
  gpio_put(cs_pin, 1);   // set CS high to end transfer
  gpio_put(cs_pin, 0);   // set CS low to start transfer
  spi_write16_blocking(spi_unit, &data_r, 1); // write right channel
  gpio_put(cs_pin, 1);   // set CS high to end transfer
  gpio_put(ldac_pin, 0); // set LDAC low to latch value
}

static inline void
psg_mcp4921_write(uint16_t l, uint16_t r)
{
  uint16_t mono = (uint16_t)(((uint32_t)l + (uint32_t)r) / 2);
  uint16_t data = 0x3000 | (mono & 0x0FFF);  // Channel A
  gpio_put(ldac_pin, 1); // set LDAC high to prepare for transfer
  gpio_put(cs_pin, 0);   // set CS low to start transfer
  spi_write16_blocking(spi_unit, &data, 1); // write mono value
  gpio_put(cs_pin, 1);   // set CS high to end transfer
  gpio_put(ldac_pin, 0); // set LDAC low to latch value
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

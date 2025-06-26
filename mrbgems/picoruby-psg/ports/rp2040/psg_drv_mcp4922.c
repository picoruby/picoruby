#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "pico/util/queue.h"

#include "../../include/psg.h"
#include "hardware/dma.h"

#include "dma_dac_write.pio.h"

#define DAC_SPI_BAUD  16000000.0f   /* 16 MHz */

static PIO pio = pio0;
static uint sm;
static int dma_chan_a, dma_chan_b;
static int dma_timer = 0; // Use DMA Timer 0

static int dma_chan_a;
static int dma_chan_b;

// Initialize MCP4922 PIO DAC
static void
psg_mcp4922_init(uint8_t ldac_pin, uint8_t _v)
{
  // --- PIO Setup (as before) ---
  uint offset = pio_add_program(pio, &dma_dac_write_program);
  sm = pio_claim_unused_sm(pio, true);
  pio_sm_config c = dma_dac_write_program_get_default_config(offset);

  uint cs_pin   = ldac_pin + 1;
  uint sck_pin  = ldac_pin + 2;
  uint copi_pin = ldac_pin + 3;

  sm_config_set_set_pins(&c, ldac_pin, 2);
  sm_config_set_out_pins(&c, copi_pin, 1);
  sm_config_set_sideset_pins(&c, sck_pin);

  // Use autopull as the PIO program does not have a 'pull' instruction.
  sm_config_set_out_shift(&c, false, true, 32);

  // Set PIO clock divider. This only affects the SPI baud rate, not the sample rate.
  // The sample rate is now controlled by the DMA timer.
  float clk_div = (float)clock_get_hz(clk_sys) / (DAC_SPI_BAUD);
  sm_config_set_clkdiv(&c, clk_div);

  for (int i=0; i<4; ++i) pio_gpio_init(pio, ldac_pin + i);
  pio_sm_set_consecutive_pindirs(pio, sm, ldac_pin, 4, true);

  pio_sm_init(pio, sm, offset, &c);
  
  // --- DMA Timer Setup ---
  // This is the key technique from the example code you found.
  // We configure a DMA timer to generate a DREQ signal at exactly the sample rate.
  dma_timer = dma_claim_unused_timer(true);
  dma_timer_set_fraction(dma_timer, SAMPLE_RATE, clock_get_hz(clk_sys));

  // --- DMA Channel Setup ---
  dma_chan_a = dma_claim_unused_channel(true);
  dma_chan_b = dma_claim_unused_channel(true);

  // Config for Channel A
  dma_channel_config cfg_a = dma_channel_get_default_config(dma_chan_a);
  channel_config_set_transfer_data_size(&cfg_a, DMA_SIZE_32);
  channel_config_set_read_increment(&cfg_a, true);
  channel_config_set_write_increment(&cfg_a, false);
  // Pace the DMA transfer using the DMA timer's DREQ signal.
  channel_config_set_dreq(&cfg_a, dma_get_timer_dreq(dma_timer));
  channel_config_set_chain_to(&cfg_a, dma_chan_b);

  dma_channel_configure(dma_chan_a, &cfg_a, &pio->txf[sm], pcm_buf, BUF_SAMPLES / 2, false);

  // Config for Channel B
  dma_channel_config cfg_b = dma_channel_get_default_config(dma_chan_b);
  channel_config_set_transfer_data_size(&cfg_b, DMA_SIZE_32);
  channel_config_set_read_increment(&cfg_b, true);
  channel_config_set_write_increment(&cfg_b, false);
  // Pace this channel using the same DMA timer DREQ.
  channel_config_set_dreq(&cfg_b, dma_get_timer_dreq(dma_timer));
  channel_config_set_chain_to(&cfg_b, dma_chan_a);
  
  dma_channel_configure(dma_chan_b, &cfg_b, &pio->txf[sm], &pcm_buf[BUF_SAMPLES / 2], BUF_SAMPLES / 2, false);

  // IRQs will be enabled by Core 1 in the start function.
}

extern queue_t fill_request_queue;

static void
dma_irq_handler()
{
  if (dma_hw->ints0 & (1u << dma_chan_a)) {
    (sio_hw->gpio_togl = 1u << 17);
    dma_hw->ints0 = (1u << dma_chan_a);
    uint8_t req = 0;
    queue_try_add(&fill_request_queue, &req); // Add request '0' to the queue
  }
  if (dma_hw->ints0 & (1u << dma_chan_b)) {
    (sio_hw->gpio_togl = 1u << 16);
    dma_hw->ints0 = (1u << dma_chan_b);
    uint8_t req = 1;
    queue_try_add(&fill_request_queue, &req); // Add request '1' to the queue
  }
}

static void
psg_mcp4922_start(void)
{
  // Ensure the IRQ handler is set for the correct core (Core 1)
  irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
  irq_set_enabled(DMA_IRQ_0, true);
  dma_channel_set_irq0_enabled(dma_chan_a, true);
  dma_channel_set_irq0_enabled(dma_chan_b, true);

  // Start the PIO state machine
  pio_sm_set_enabled(pio, sm, true);

  // Start the first DMA transfer, which will be paced by the DMA timer.
  dma_channel_start(dma_chan_a);
}

static void
psg_mcp4922_stop(void)
{
  if (dma_channel_is_busy(dma_chan_a) || dma_channel_is_busy(dma_chan_b)) {
    dma_channel_abort(dma_chan_a);
    dma_channel_abort(dma_chan_b);
  }
  dma_timer_unclaim(dma_timer);
  dma_channel_unclaim(dma_chan_a);
  dma_channel_unclaim(dma_chan_b);
  pio_sm_set_enabled(pio, sm, false);
  // Optional: remove program from PIO memory
}

static inline void
psg_mcp4922_write(uint16_t l, uint16_t r)
{
//  mcp4922_pio_write_blocking(&dac_config, l, r);
}

const psg_output_api_t psg_drv_mcp4922 = {
  .init  = psg_mcp4922_init,
  .start = psg_mcp4922_start,
  .stop  = psg_mcp4922_stop,
  .write = psg_mcp4922_write
};


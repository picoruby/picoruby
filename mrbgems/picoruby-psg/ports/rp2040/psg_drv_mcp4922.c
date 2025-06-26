#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"

#include "pico/util/queue.h"

#include "../../include/psg.h"
#include "dma_dac_write.pio.h"

// --- Globals for this driver ---
static PIO pio = pio0;
static uint sm;
static int dma_data_chan;
static int dma_ctrl_chan;
static int dma_timer;

static uint32_t* pcm_buf_a_ptr = &pcm_buf[0];
static uint32_t* pcm_buf_b_ptr = &pcm_buf[BUF_SAMPLES / 2];

// This public function must be called from the IRQ handler context.
void dma_irq_feed_and_repoint(queue_t *q);

static void psg_mcp4922_init(uint8_t ldac_pin, uint8_t _v) {
  // --- PIO Setup ---
  uint offset = pio_add_program(pio, &dma_dac_write_program);
  sm = pio_claim_unused_sm(pio, true);
  pio_sm_config c = dma_dac_write_program_get_default_config(offset);
  
  uint copi_pin = ldac_pin + 3;
  uint sck_pin  = ldac_pin + 2;
  sm_config_set_set_pins(&c, ldac_pin, 2);
  sm_config_set_out_pins(&c, copi_pin, 1);
  sm_config_set_sideset_pins(&c, sck_pin);
  sm_config_set_out_shift(&c, false, true, 32);
  float clk_div = 4.0f;
  sm_config_set_clkdiv(&c, clk_div);
  for (int i=0; i<4; ++i) pio_gpio_init(pio, ldac_pin + i);
  pio_sm_set_consecutive_pindirs(pio, sm, ldac_pin, 4, true);
  pio_sm_init(pio, sm, offset, &c);
  
  // --- DMA Timer Setup ---
  dma_timer = dma_claim_unused_timer(true);
  uint32_t denominator = clock_get_hz(clk_sys) / SAMPLE_RATE;
  dma_timer_set_fraction(dma_timer, 1, denominator);

  // --- DMA Channel Setup ---
  dma_data_chan = dma_claim_unused_channel(true);
  dma_ctrl_chan = dma_claim_unused_channel(true);

  // Data Channel: plays buffer, chains to control channel
  dma_channel_config data_cfg = dma_channel_get_default_config(dma_data_chan);
  channel_config_set_transfer_data_size(&data_cfg, DMA_SIZE_32);
  channel_config_set_read_increment(&data_cfg, true);
  channel_config_set_write_increment(&data_cfg, false);
  channel_config_set_dreq(&data_cfg, dma_get_timer_dreq(dma_timer));
  channel_config_set_chain_to(&data_cfg, dma_ctrl_chan);
  dma_channel_configure(dma_data_chan, &data_cfg, &pio->txf[sm], pcm_buf, BUF_SAMPLES / 2, false);

  // Control Channel: points data channel to next buffer and triggers IRQ
  dma_channel_config ctrl_cfg = dma_channel_get_default_config(dma_ctrl_chan);
  channel_config_set_transfer_data_size(&ctrl_cfg, DMA_SIZE_32);
  channel_config_set_read_increment(&ctrl_cfg, false);
  channel_config_set_write_increment(&ctrl_cfg, false);
  dma_channel_configure(dma_ctrl_chan, &ctrl_cfg, &dma_hw->ch[dma_data_chan].al3_read_addr_trig, &pcm_buf_b_ptr, 1, false);
}

static void
psg_mcp4922_start(void (*dma_irq_handler_ptr)(void))
{
  // Set the IRQ handler provided by the caller (which is running on the correct core).
  irq_set_priority(DMA_IRQ_0, 0x00);
  dma_channel_set_irq0_enabled(dma_ctrl_chan, true);
  irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler_ptr);
  irq_set_enabled(DMA_IRQ_0, true);
  
  pio_sm_set_enabled(pio, sm, true);
  dma_channel_start(dma_data_chan);
}

static void
psg_mcp4922_stop(void)
{
  if (dma_channel_is_busy(dma_data_chan) || dma_channel_is_busy(dma_ctrl_chan)) {
    dma_channel_abort(dma_data_chan);
    dma_channel_abort(dma_ctrl_chan);
  }
  dma_timer_unclaim(dma_timer);
  dma_channel_unclaim(dma_data_chan);
  dma_channel_unclaim(dma_ctrl_chan);
  pio_sm_set_enabled(pio, sm, false);
}

// Public function to be called from the IRQ handler on Core 1
static volatile int now_playing_buf = 0; // 0=A, 1=B
void dma_irq_feed_and_repoint(queue_t *q) {
  dma_hw->ints0 = (1u << dma_ctrl_chan);
  uint8_t req = now_playing_buf;
  queue_try_add(q, &req);
  if (now_playing_buf == 0) {
    dma_channel_set_read_addr(dma_ctrl_chan, &pcm_buf_b_ptr, false);
  } else {
    dma_channel_set_read_addr(dma_ctrl_chan, &pcm_buf_a_ptr, false);
  }
  now_playing_buf = 1 - now_playing_buf;
}

const psg_output_api_t psg_drv_mcp4922 = {
  .init  = psg_mcp4922_init,
  .start = psg_mcp4922_start,
  .stop  = psg_mcp4922_stop,
  .write = NULL
};


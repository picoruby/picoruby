#include "pico/stdlib.h"
#include "hardware/pwm.h"

#ifndef PSG_PWM_DMA
#define PSG_PWM_DMA 0
#endif

#if PSG_PWM_DMA
#include "hardware/clocks.h"
#include "hardware/dma.h"
#endif

#include "../../include/psg.h"

#define PWM_STOP_SETTLE_US 50

static uint slice_num;
static bool pwm_initialized = false;
static bool left_is_a = true;

#if PSG_PWM_DMA
static int dma_chan = -1;
static dma_channel_config dma_config;
static uint32_t dma_buf[PSG_BUFFER_OUTPUT_SAMPLES];
#endif

static void
psg_pwm_shutdown(void)
{
  if (!pwm_initialized) return;

#if PSG_PWM_DMA
  if (dma_chan >= 0) {
    dma_channel_abort(dma_chan);
    dma_channel_cleanup(dma_chan);
    dma_channel_unclaim(dma_chan);
    dma_chan = -1;
  }
#endif

  pwm_set_both_levels(slice_num, 0, 0);
  sleep_us(PWM_STOP_SETTLE_US);
  pwm_set_enabled(slice_num, false);
  pwm_initialized = false;
}

static void
psg_pwm_init(uint8_t lpin, uint8_t rpin)
{
  uint l_gpio = (uint)lpin;
  uint r_gpio = (uint)rpin;
  uint l_slice = pwm_gpio_to_slice_num(l_gpio);
  uint r_slice = pwm_gpio_to_slice_num(r_gpio);

  psg_pwm_shutdown();

  if (l_slice != r_slice) {
    // ToDo: Should raise an exception
    return;
  }

  gpio_set_function(l_gpio, GPIO_FUNC_PWM);
  gpio_set_function(r_gpio, GPIO_FUNC_PWM);

  slice_num = l_slice;
  left_is_a = (pwm_gpio_to_channel(l_gpio) == PWM_CHAN_A);

  /* common settings ------------------------------------------------ */
  pwm_set_wrap(slice_num, MAX_SAMPLE_WIDTH);

#if PSG_PWM_DMA
  pwm_set_clkdiv(slice_num, (float)clock_get_hz(clk_sys) / (SAMPLE_RATE * (MAX_SAMPLE_WIDTH + 1.0f)));
#else
  /* optional: div = 1.0 (125 MHz) -> 30.5 kHz carrier @ wrap 4095   */
  pwm_set_clkdiv_int_frac(slice_num, 1, 0);
#endif

  /* start with zero level to avoid pop                               */
  pwm_set_both_levels(slice_num, 0, 0);

#if PSG_PWM_DMA
  dma_chan = dma_claim_unused_channel(true);
  dma_config = dma_channel_get_default_config(dma_chan);
  channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
  channel_config_set_read_increment(&dma_config, true);
  channel_config_set_write_increment(&dma_config, false);
  channel_config_set_dreq(&dma_config, pwm_get_dreq(slice_num));
  dma_channel_configure(
    dma_chan,
    &dma_config,
    &pwm_hw->slice[slice_num].cc,
    NULL,
    0,
    false
  );
#endif

  pwm_set_enabled(slice_num, true);
  pwm_initialized = true;
}

static void
psg_pwm_start(void)
{
  if (pwm_initialized) pwm_set_enabled(slice_num, true);
}

static void
psg_pwm_stop(void)
{
  psg_pwm_shutdown();
}

static void
psg_pwm_write(uint16_t l, uint16_t r)
{
  if (!pwm_initialized) return;

  if (left_is_a) {
    pwm_set_both_levels(slice_num, l, r);
  } else {
    pwm_set_both_levels(slice_num, r, l);
  }
}

#if PSG_PWM_DMA
static inline uint32_t
psg_pwm_pack_sample(uint32_t sample)
{
  uint16_t l = (sample >> 16) & MAX_SAMPLE_WIDTH;
  uint16_t r = sample & MAX_SAMPLE_WIDTH;
  return left_is_a ? (((uint32_t)r << 16) | l) : (((uint32_t)l << 16) | r);
}

static bool
psg_pwm_write_buffer(const uint32_t *samples, uint32_t count)
{
  if (!pwm_initialized || dma_chan < 0 || count > PSG_BUFFER_OUTPUT_SAMPLES) return false;
  if (dma_channel_is_busy(dma_chan)) return false;

  for (uint32_t i = 0; i < count; i++) {
    dma_buf[i] = psg_pwm_pack_sample(samples[i]);
  }

  dma_channel_configure(
    dma_chan,
    &dma_config,
    &pwm_hw->slice[slice_num].cc,
    dma_buf,
    count,
    true
  );
  return true;
}
#endif

const psg_output_api_t psg_drv_pwm = {
  .init         = psg_pwm_init,
  .start        = psg_pwm_start,
  .stop         = psg_pwm_stop,
  .write        = psg_pwm_write,
#if PSG_PWM_DMA
  .write_buffer = psg_pwm_write_buffer
#else
  .write_buffer = NULL
#endif
};

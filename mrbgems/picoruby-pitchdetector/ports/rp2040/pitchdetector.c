#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "../../include/pitchdetector.h"

uint16_t buffer_a[BUFFER_SIZE];
uint16_t buffer_b[BUFFER_SIZE];
uint16_t *active_buffer;
volatile bool buffer_ready = false;
static uint dma_chan;
static dma_channel_config dma_config;
static uint16_t volume_threshold;

// RMS calculation for volume detection
static float
calculate_rms(uint16_t *buffer, int size)
{
 float sum = 0;
 float mean = 0;
 
 // Calculate mean value (DC offset)
 for (int i = 0; i < size; i++) {
   mean += buffer[i];
 }
 mean /= size;
 
 // Calculate RMS
 for (int i = 0; i < size; i++) {
   float diff = buffer[i] - mean;
   sum += diff * diff;
 }
 
 return sqrt(sum / size);
}

static float
autocorrelation(uint16_t *buffer, int lag)
{
 float sum = 0;
 float norm_a = 0;
 float norm_b = 0;
 
 // Calculate mean for DC offset removal
 float mean = 0;
 for (int i = 0; i < BUFFER_SIZE; i++) {
   mean += buffer[i];
 }
 mean /= BUFFER_SIZE;
 
 // Calculate autocorrelation
 for (int i = 0; i < BUFFER_SIZE - lag; i++) {
   float a = buffer[i] - mean;
   float b = buffer[i + lag] - mean;
   sum += a * b;
   norm_a += a * a;
   norm_b += b * b;
 }
 
 if (norm_a * norm_b == 0) return 0;
 return sum / sqrt(norm_a * norm_b);
}

// Pitch detection using autocorrelation
float
detect_pitch(uint16_t *buffer)
{
 // Check volume_threshold first
 float rms = calculate_rms(buffer, BUFFER_SIZE);
 if (rms < volume_threshold) {
   return 0;  // Too quiet, ignore
 }
 
 int min_lag = SAMPLE_RATE / 800;   // 800Hz (highest)
 int max_lag = SAMPLE_RATE / 80;    // 80Hz (lowest)
 
 float max_corr = 0;
 int best_lag = 0;
 
 // Find the best correlation lag
 for (int lag = min_lag; lag < max_lag && lag < BUFFER_SIZE/2; lag++) {
   float corr = autocorrelation(buffer, lag);
   
   // Peak detection
   if (corr > max_corr && corr > 0.3) {
     max_corr = corr;
     best_lag = lag;
   }
 }
 
 if (best_lag == 0) return 0;
 
 // Calculate frequency
 return (float)SAMPLE_RATE / best_lag;
}

// DMA interrupt handler
static void
dma_handler()
{
  // Clear interrupt
  dma_hw->ints0 = 1u << dma_chan;

  // Switch buffers
  if (active_buffer == buffer_a) {
    active_buffer = buffer_b;
    dma_channel_configure(dma_chan, &dma_config,
      buffer_b,          // Write destination
      &adc_hw->fifo,     // Read source
      BUFFER_SIZE,       // Transfer count
      true               // Start immediately
    );
  } else {
    active_buffer = buffer_a;
    dma_channel_configure(dma_chan, &dma_config,
      buffer_a,          // Write destination
      &adc_hw->fifo,     // Read source
      BUFFER_SIZE,       // Transfer count
      true               // Start immediately
    );
  }

  buffer_ready = true;
}

void
PITCHDETECTOR_set_volume_threshold(uint16_t value)
{
  volume_threshold = value;
}

void
PITCHDETECTOR_start(uint8_t input)
{
  // Select ADC input channel
  adc_select_input(input);

  // Setup ADC FIFO
  adc_fifo_setup(
    true,    // Enable FIFO
    true,    // Enable DMA request
    1,       // DREQ assertion threshold
    false,   // Don't include error bit
    false    // No byte shift
  );

  // Set sampling rate
  adc_set_clkdiv(96000000.0 / SAMPLE_RATE);

  // Claim DMA channel
  dma_chan = dma_claim_unused_channel(true);
  dma_config = dma_channel_get_default_config(dma_chan);

  // Configure DMA
  channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_16);
  channel_config_set_read_increment(&dma_config, false);
  channel_config_set_write_increment(&dma_config, true);
  channel_config_set_dreq(&dma_config, DREQ_ADC);

  // Setup first transfer
  active_buffer = buffer_a;
  dma_channel_configure(dma_chan, &dma_config,
    buffer_a,          // Write destination
    &adc_hw->fifo,     // Read source
    BUFFER_SIZE,       // Transfer count
    false              // Don't start yet
  );

  // Enable DMA interrupt
  dma_channel_set_irq0_enabled(dma_chan, true);
  irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
  irq_set_enabled(DMA_IRQ_0, true);

  // Start ADC and DMA
  adc_run(true);
  dma_channel_start(dma_chan);
}

void
PITCHDETECTOR_stop(void)
{
 // Stop ADC
 adc_run(false);
 adc_fifo_drain();
 
 // Stop DMA
 dma_channel_abort(dma_chan);
 
 // Disable interrupts
 irq_set_enabled(DMA_IRQ_0, false);
 dma_channel_set_irq0_enabled(dma_chan, false);
 
 // Release DMA channel
 dma_channel_unclaim(dma_chan);
 
 // Disable ADC FIFO
 adc_fifo_setup(
   false,   // Disable FIFO
   false,   // Disable DMA request
   0,       // No DREQ threshold
   false,   // Don't include error bit
   false    // No byte shift
 );
}

float
PITCHDETECTOR_detect_pitch(void)
{
 if (buffer_ready) {
   buffer_ready = false;
   
   // Process the inactive buffer
   uint16_t *process_buffer = (active_buffer == buffer_a) ? buffer_b : buffer_a;
   
   float frequency = detect_pitch(process_buffer);
   return frequency;
 }
 
 return -1.0;  // No data ready
}

#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "../../include/pitchdetector.h"

// RP2040 hardware buffers
uint16_t buffer_a[BUFFER_SIZE];
uint16_t buffer_b[BUFFER_SIZE];
uint16_t *active_buffer;
volatile bool buffer_ready = false;

// DMA configuration
static uint dma_chan;
static dma_channel_config dma_config;

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
PITCHDETECTOR_start(uint8_t input)
{
  // Initialize processing subsystem
  PITCHDETECTOR_init_processing();
  
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
   
   // Use platform-independent processing
   float frequency = PITCHDETECTOR_process_buffer(process_buffer, BUFFER_SIZE);
   return frequency;
 }
 
 return -1.0;  // No data ready
}
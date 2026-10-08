/*
 * nRF52 port: SAADC through nrfx_saadc.
 *
 * Only eight pins on the nRF52840 reach the SAADC, and each is wired to
 * a fixed analog input -- there is no mux to point an arbitrary GPIO at
 * the converter. ADC_init therefore accepts exactly those eight and
 * rejects everything else rather than silently returning a channel that
 * reads noise.
 *
 * The driver rather than the HAL, because nrfx_saadc_sample_convert is
 * exactly this gem's contract: it selects the input, converts, waits,
 * and hands back the value. It also saves and restores the other
 * channels' inputs around the conversion and bounds its own wait, both
 * of which a hand-written START/SAMPLE/END loop has to get right on its
 * own and neither of which it gets for free.
 *
 * Reference and gain are the usual pairing: the internal 0.6 V reference
 * with a 1/6 gain puts full scale at 3.6 V, which covers the 3.3 V rail
 * the board runs at with headroom instead of clipping just below it.
 * They are written out rather than taken from
 * NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE so that the conversion this port
 * documents does not change under it when sdk_config.h does.
 */

#include <stdint.h>
#include <stdbool.h>

#include "nrfx_saadc.h"

#include "../../include/adc.h"

#define ADC_CHANNEL_COUNT 8
#define ADC_RESOLUTION    4095            /* 12-bit, single-ended */
#define ADC_VOLTAGE_MAX   3.6             /* 0.6 V reference / (1/6) gain */

/* Priority 6, as app_timer and the USB driver use: a conversion must not
   preempt either. */
#define ADC_IRQ_PRIORITY  6

/* AIN0..AIN7 in pin order. The nRF52840 wires these and nothing else. */
static const uint8_t channel_pin[ADC_CHANNEL_COUNT] = {
  2, 3, 4, 5, 28, 29, 30, 31
};

static bool channel_ready[ADC_CHANNEL_COUNT];
static bool driver_ready;

/* Set from the SAADC interrupt, read by the loop below. */
static volatile bool calibration_done;

/*
 * Offset auto-calibration completes through the driver's event handler,
 * so a handler has to exist even though every conversion this port makes
 * is blocking and never reports through it.
 */
static void
saadc_event_handler(nrfx_saadc_evt_t const *p_event)
{
  if (p_event != NULL && p_event->type == NRFX_SAADC_EVT_CALIBRATEDONE) {
    calibration_done = true;
  }
}

int
ADC_pin_num_from_char(const uint8_t *str)
{
  /*
   * Not supported. The RP2040 port answers "temperature" here because
   * its die sensor is an ADC input; on nRF52 the temperature sensor is
   * a separate peripheral (NRF_TEMP) that reports quarter-degrees, not
   * a voltage, so it cannot honestly be reached through ADC#read_voltage.
   */
  (void)str;
  return -1;
}

static bool
driver_start(void)
{
  if (driver_ready) {
    return true;
  }

  nrfx_saadc_config_t config = {
    .resolution         = NRF_SAADC_RESOLUTION_12BIT,
    .oversample         = NRF_SAADC_OVERSAMPLE_DISABLED,
    .interrupt_priority = ADC_IRQ_PRIORITY,
    .low_power_mode     = false,
  };
  if (nrfx_saadc_init(&config, saadc_event_handler) != NRFX_SUCCESS) {
    return false;
  }
  driver_ready = true;

  /*
   * Offset auto-calibration, once. Without it every reading carries a
   * DC error that is worst near 0 V -- where a single-ended channel
   * reads negative and the clamp in ADC_read_raw quietly turns it into
   * zero, so nothing about the output reveals the miscalibration.
   *
   * Bounded: calibration is tens of microseconds, and a SAADC that never
   * raises CALIBRATEDONE must not take the whole VM down with it. An
   * uncalibrated converter still reads, just less accurately near zero.
   */
  calibration_done = false;
  if (nrfx_saadc_calibrate_offset() == NRFX_SUCCESS) {
    for (uint32_t spins = 0; spins < 1000000u && !calibration_done; spins++) {
    }
  }
  return true;
}

int
ADC_init(uint8_t pin)
{
  int channel = -1;

  for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
    if (channel_pin[i] == pin) {
      channel = i;
      break;
    }
  }
  if (channel < 0) {
    return -1;  /* pin has no analog input behind it */
  }
  if (!driver_start()) {
    return -1;
  }
  if (channel_ready[channel]) {
    return channel;
  }

  nrf_saadc_channel_config_t config = {
    .resistor_p = NRF_SAADC_RESISTOR_DISABLED,
    .resistor_n = NRF_SAADC_RESISTOR_DISABLED,
    .gain       = NRF_SAADC_GAIN1_6,
    .reference  = NRF_SAADC_REFERENCE_INTERNAL,
    .acq_time   = NRF_SAADC_ACQTIME_10US,
    .mode       = NRF_SAADC_MODE_SINGLE_ENDED,
    .burst      = NRF_SAADC_BURST_DISABLED,
    .pin_p      = (nrf_saadc_input_t)(NRF_SAADC_INPUT_AIN0 + channel),
    .pin_n      = NRF_SAADC_INPUT_DISABLED,
  };
  if (nrfx_saadc_channel_init((uint8_t)channel, &config) != NRFX_SUCCESS) {
    return -1;
  }
  channel_ready[channel] = true;
  return channel;
}

uint32_t
ADC_read_raw(uint8_t input)
{
  nrf_saadc_value_t value = 0;

  if (ADC_CHANNEL_COUNT <= input || !channel_ready[input]) {
    return 0;
  }
  if (nrfx_saadc_sample_convert(input, &value) != NRFX_SUCCESS) {
    return 0;
  }
  /* Single-ended conversions are still signed, and a reading slightly
     below the reference floor comes back negative. Clamp: the contract
     returns unsigned, and wrapping would report a huge value. */
  if (value < 0) {
    return 0;
  }
  return (uint32_t)value;
}

#ifndef PICORB_NO_FLOAT
picorb_float_t
ADC_read_voltage(uint8_t input)
{
  return (picorb_float_t)ADC_read_raw(input) * ADC_VOLTAGE_MAX / ADC_RESOLUTION;
}
#endif

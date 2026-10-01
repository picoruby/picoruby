/*
 * nRF52 port: SAADC, one channel per pin.
 *
 * Only eight pins on the nRF52840 reach the SAADC, and each is wired
 * to a fixed analog input -- there is no mux to point an arbitrary GPIO
 * at the converter. ADC_init therefore accepts exactly those eight and
 * rejects everything else rather than silently returning a channel that
 * reads noise.
 *
 * Reference and gain are the usual pairing: the internal 0.6 V
 * reference with a 1/6 gain puts full scale at 3.6 V, which covers the
 * 3.3 V rail the board runs at with headroom instead of clipping just
 * below it.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "nrf.h"
#include "nrf_saadc.h"

#include "../../include/adc.h"

#define ADC_CHANNEL_COUNT 8
#define ADC_RESOLUTION    4095            /* 12-bit, single-ended */
#define ADC_VOLTAGE_MAX   3.6             /* 0.6 V reference / (1/6) gain */

/* AIN0..AIN7 in pin order. The nRF52840 wires these and nothing else. */
static const uint8_t channel_pin[ADC_CHANNEL_COUNT] = {
  2, 3, 4, 5, 28, 29, 30, 31
};

static bool channel_ready[ADC_CHANNEL_COUNT];
static nrf_saadc_channel_config_t channel_config[ADC_CHANNEL_COUNT];

/* EasyDMA writes the result here, so it has to be in RAM and has to
   outlive the conversion. volatile because the CPU is not the writer. */
static volatile nrf_saadc_value_t result_buffer;

/*
 * TASKS_SAMPLE converts every channel whose PSELP is set -- scan mode is
 * implicit, not something this port opted into. With a one-entry result
 * buffer, EVENTS_END then fires as soon as the lowest-numbered channel
 * has landed, so a second ADC.new() would make every read return the
 * first pin's voltage. Keep exactly one channel selected at a time and
 * install the wanted one just before sampling.
 */
static void
select_only(uint8_t channel)
{
  for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
    if (i == (int)channel) {
      nrf_saadc_channel_init((uint8_t)i, &channel_config[i]);
    } else {
      nrf_saadc_channel_input_set((uint8_t)i,
                                  NRF_SAADC_INPUT_DISABLED,
                                  NRF_SAADC_INPUT_DISABLED);
    }
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

  nrf_saadc_channel_config_t config = {
    .resistor_p = NRF_SAADC_RESISTOR_DISABLED,
    .resistor_n = NRF_SAADC_RESISTOR_DISABLED,
    .gain       = NRF_SAADC_GAIN1_6,
    .reference  = NRF_SAADC_REFERENCE_INTERNAL,
    /* 10 us settles the sample-and-hold for source impedances up to
       about 100 kOhm, which is the common case for a divider or a
       potentiometer. */
    .acq_time   = NRF_SAADC_ACQTIME_10US,
    .mode       = NRF_SAADC_MODE_SINGLE_ENDED,
    .burst      = NRF_SAADC_BURST_DISABLED,
    .pin_p      = (nrf_saadc_input_t)(NRF_SAADC_INPUT_AIN0 + channel),
    .pin_n      = NRF_SAADC_INPUT_DISABLED,
  };

  channel_config[channel] = config;
  channel_ready[channel] = true;

  static bool calibrated = false;
  if (!calibrated) {
    nrf_saadc_resolution_set(NRF_SAADC_RESOLUTION_12BIT);
    nrf_saadc_enable();
    /* Offset auto-calibration, once. Without it every reading carries a
       DC error that is worst near 0 V -- where a single-ended channel
       reads negative and the clamp below quietly turns it into zero, so
       nothing about the output reveals the miscalibration. */
    nrf_saadc_event_clear(NRF_SAADC_EVENT_CALIBRATEDONE);
    nrf_saadc_task_trigger(NRF_SAADC_TASK_CALIBRATEOFFSET);
    while (!nrf_saadc_event_check(NRF_SAADC_EVENT_CALIBRATEDONE)) {
    }
    nrf_saadc_event_clear(NRF_SAADC_EVENT_CALIBRATEDONE);
    calibrated = true;
  }
  return channel;
}

uint32_t
ADC_read_raw(uint8_t input)
{
  if (ADC_CHANNEL_COUNT <= input || !channel_ready[input]) {
    return 0;
  }

  /*
   * One conversion per call, with the buffer re-armed each time.
   * TASKS_START hands EasyDMA the buffer; TASKS_SAMPLE performs the
   * conversion; END fires once the result has landed in RAM.
   */
  select_only(input);

  result_buffer = 0;
  nrf_saadc_buffer_init((nrf_saadc_value_t *)&result_buffer, 1);

  nrf_saadc_event_clear(NRF_SAADC_EVENT_STARTED);
  nrf_saadc_task_trigger(NRF_SAADC_TASK_START);
  while (!nrf_saadc_event_check(NRF_SAADC_EVENT_STARTED)) {
  }
  nrf_saadc_event_clear(NRF_SAADC_EVENT_STARTED);

  nrf_saadc_event_clear(NRF_SAADC_EVENT_END);
  nrf_saadc_task_trigger(NRF_SAADC_TASK_SAMPLE);
  while (!nrf_saadc_event_check(NRF_SAADC_EVENT_END)) {
  }
  nrf_saadc_event_clear(NRF_SAADC_EVENT_END);

  /* Single-ended conversions are still signed, and a reading slightly
     below the reference floor comes back negative. Clamp: the contract
     returns unsigned, and wrapping would report a huge value. */
  if (result_buffer < 0) {
    return 0;
  }
  return (uint32_t)result_buffer;
}

#ifndef PICORB_NO_FLOAT
picorb_float_t
ADC_read_voltage(uint8_t input)
{
  return (picorb_float_t)ADC_read_raw(input) * ADC_VOLTAGE_MAX / ADC_RESOLUTION;
}
#endif

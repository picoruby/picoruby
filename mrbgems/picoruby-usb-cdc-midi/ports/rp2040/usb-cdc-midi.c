#include <stddef.h>
#include <stdint.h>
#include <tusb.h>

#include "../../include/usb-cdc-midi.h"

#define MIDI_CDC_INSTANCE 2

bool
usb_cdc_midi_connected(void)
{
  return tud_cdc_n_connected(MIDI_CDC_INSTANCE);
}

size_t
usb_cdc_midi_write(const uint8_t *data, size_t length)
{
  uint32_t written = tud_cdc_n_write(MIDI_CDC_INSTANCE, data, (uint32_t)length);
  tud_cdc_n_write_flush(MIDI_CDC_INSTANCE);
  return (size_t)written;
}

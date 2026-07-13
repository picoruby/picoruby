#include <stddef.h>
#include <stdint.h>
#include "../../../picoruby-machine/include/hal.h"
#include "../../include/usb-cdc-midi.h"

#define MIDI_CDC_INSTANCE 2

bool
usb_cdc_midi_connected(void)
{
  return picorb_hal_cdc_connected(MIDI_CDC_INSTANCE);
}

size_t
usb_cdc_midi_write(const uint8_t *data, size_t length)
{
  int written = picorb_hal_cdc_write(MIDI_CDC_INSTANCE, data, (int)length, 0);
  return written < 0 ? 0 : (size_t)written;
}

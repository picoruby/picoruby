#include "../include/usb-cdc-midi.h"

#if defined(PICORB_VM_MRUBY)
#include "mruby/usb-cdc-midi.c"
#endif

/* Host builds use these fallbacks. The RP2 firmware links strong definitions
 * from ports/rp2040/usb-cdc-midi.c. */
__attribute__((weak)) bool
usb_cdc_midi_connected(void)
{
  return false;
}

__attribute__((weak)) size_t
usb_cdc_midi_write(const uint8_t *data, size_t length)
{
  (void)data;
  (void)length;
  return 0;
}

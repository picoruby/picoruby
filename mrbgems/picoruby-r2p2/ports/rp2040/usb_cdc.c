#include <pico/stdlib.h>
#include <tusb.h>
#include "hal.h"

//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
  (void) itf;
  (void) rts;

  // TODO set some indicator
  if ( dtr )
  {
    // Terminal connected
  }else
  {
    // Terminal disconnected
  }
}

// Invoked when CDC interface received data from host
// Called from USB IRQ context (usb_irq_handler -> tud_task)
void tud_cdc_rx_cb(uint8_t itf)
{
  (void) itf;
  while (tud_cdc_available()) {
    uint8_t ch = (uint8_t)tud_cdc_read_char();
    hal_stdin_push(ch);
  }
}

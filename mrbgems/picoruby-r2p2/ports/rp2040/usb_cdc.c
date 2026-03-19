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
// Called from tud_task() when new CDC data is available.
//
// Only transfer bytes while the stdin ring buffer has space.
// When the ring buffer is full we stop reading from the CDC FIFO so
// remaining bytes stay safely in TinyUSB's 512-byte software FIFO.
// They will be drained on the next hal_getchar() call after the
// application has consumed some ring buffer bytes.
//
// Previously this function drained the ENTIRE CDC FIFO regardless of
// ring buffer capacity, causing silent data loss during large bulk
// transfers (e.g. DFU uploads > 4 KB).
void tud_cdc_rx_cb(uint8_t itf)
{
  (void) itf;
  while (tud_cdc_available()) {
    uint8_t ch = (uint8_t)tud_cdc_read_char();
    if (!hal_stdin_push(ch)) {
      // Ring buffer full.  The byte we just read from the CDC FIFO is
      // lost (TinyUSB has no un-read API).  Break immediately to
      // preserve the remaining bytes in the CDC FIFO.
      // With PICORB_STDIN_BUFFER_SIZE >= CFG_TUD_CDC_RX_BUFSIZE
      // this path should never be reached in practice.
      break;
    }
  }
}

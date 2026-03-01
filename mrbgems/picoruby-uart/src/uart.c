#include <stdbool.h>
#include <stddef.h>
#include "../include/uart.h"

#ifndef PICORUBY_UART_RX_BUFFER_SIZE
#define PICORUBY_UART_RX_BUFFER_SIZE 1024
#endif

/* Called by ports (backward-compatible wrapper) */
bool
UART_pushBuffer(RingBuffer *ring_buffer, uint8_t ch)
{
  return RingBuffer_push(ring_buffer, ch);
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/uart.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/uart.c"

#endif

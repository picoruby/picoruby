#include <stdbool.h>
#include <stddef.h>
#include "../include/uart.h"

/*
 * RingBuffer
 */

#ifndef PICORUBY_UART_RX_BUFFER_SIZE
#define PICORUBY_UART_RX_BUFFER_SIZE 1024
#endif

static bool
isPowerOfTwo(int n) {
  if (n == 0) {
    return false;
  }
  while (n != 1) {
    if (n % 2 != 0) {
      return false;
    }
    n = n / 2;
  }
  return true;
}

static bool
initializeBuffer(RingBuffer *ring_buffer, size_t size)
{
  if (!isPowerOfTwo(size)) {
    return false;
  }
  (*ring_buffer).head = 0;
  (*ring_buffer).tail = 0;
  (*ring_buffer).size = size;
  (*ring_buffer).mask = (int)(size - 1);
  return true;
}

static size_t
bufferDataSize(RingBuffer *ring_buffer)
{
  return (size_t)((ring_buffer->tail - ring_buffer->head) & ring_buffer->mask);
}

static void
clearBuffer(RingBuffer *ring_buffer)
{
  ring_buffer->head = 0;
  ring_buffer->tail = 0;
}

static size_t
bufferFreeSize(RingBuffer *ring_buffer) {
  return ring_buffer->size - bufferDataSize(ring_buffer);
}

static bool
popBuffer(RingBuffer *ring_buffer, uint8_t *popData, size_t len) {
  if (len > ring_buffer->size) {
    return false;
  }
  if (ring_buffer->tail == ring_buffer->head) {
    return false;
  }
  int remaining = bufferDataSize(ring_buffer);
  if (remaining < len) {
    len = remaining;
  }
  for (int i = 0; i < len; i++) {
    popData[i] = ring_buffer->data[ring_buffer->head];
    ring_buffer->head = (ring_buffer->head + 1) & ring_buffer->mask;
  }
  return true;
}

static int
searchCharBuffer(RingBuffer *ring_buffer, uint8_t c)
{
  int i;
  for (i = 0; i < bufferDataSize(ring_buffer); i++) {
    if (ring_buffer->data[(ring_buffer->head + i) & ring_buffer->mask] == c) {
      return i;
    }
  }
  return -1;
}

/* Called by ports */
bool
UART_pushBuffer(RingBuffer *ring_buffer, uint8_t ch)
{
  if (bufferFreeSize(ring_buffer) < 1) {
    return false;
  }
  ring_buffer->data[ring_buffer->tail] = ch;
  ring_buffer->tail = (ring_buffer->tail + 1) & ring_buffer->mask;
  return true;
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/uart.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/uart.c"

#endif

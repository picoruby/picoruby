#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "../include/uart.h"
#include "machine.h"

#ifndef PICORB_UART_RX_BUFFER_SIZE
#define PICORB_UART_RX_BUFFER_SIZE (1 << 8)
#endif

#if PICORB_UART_RX_BUFFER_SIZE <= 0 || \
    (PICORB_UART_RX_BUFFER_SIZE & (PICORB_UART_RX_BUFFER_SIZE - 1)) != 0
#error "PICORB_UART_RX_BUFFER_SIZE must be a positive power of two"
#endif

typedef struct {
  uint32_t last_read_timestamp_us;
  uint32_t overflow_count;
  bool last_read_timestamp_valid;
} uart_rx_metadata;

static size_t
timestamp_offset(size_t capacity)
{
  size_t offset = sizeof(RingBuffer) + capacity;
  size_t alignment = sizeof(uint32_t);
  return (offset + alignment - 1) & ~(alignment - 1);
}

static uint32_t *
rx_timestamps(RingBuffer *ring_buffer)
{
  return (uint32_t *)((uint8_t *)ring_buffer + timestamp_offset(ring_buffer->size));
}

static uart_rx_metadata *
rx_metadata(RingBuffer *ring_buffer)
{
  return (uart_rx_metadata *)(rx_timestamps(ring_buffer) + ring_buffer->size);
}

size_t
UART_rx_buffer_allocation_size(size_t capacity)
{
  if (capacity == 0 || (capacity & (capacity - 1)) != 0) {
    return 0;
  }
  size_t alignment_padding = sizeof(uint32_t) - 1;
  if (SIZE_MAX - sizeof(RingBuffer) - alignment_padding < capacity) {
    return 0;
  }
  size_t offset = timestamp_offset(capacity);
  if (SIZE_MAX - sizeof(uart_rx_metadata) < offset ||
      (SIZE_MAX - offset - sizeof(uart_rx_metadata)) / sizeof(uint32_t) < capacity) {
    return 0;
  }
  return offset + capacity * sizeof(uint32_t) + sizeof(uart_rx_metadata);
}

bool
UART_rx_buffer_init(RingBuffer *ring_buffer, size_t capacity)
{
  if (UART_rx_buffer_allocation_size(capacity) == 0 ||
      !RingBuffer_init(ring_buffer, capacity)) {
    return false;
  }
  memset(rx_timestamps(ring_buffer), 0, capacity * sizeof(uint32_t));
  memset(rx_metadata(ring_buffer), 0, sizeof(uart_rx_metadata));
  return true;
}

bool
UART_pushBufferAt(RingBuffer *ring_buffer, uint8_t ch, uint32_t timestamp_us)
{
  uart_rx_metadata *metadata = rx_metadata(ring_buffer);
  if (RingBuffer_free_size(ring_buffer) <= 1) {
    metadata->overflow_count++;
    return false;
  }
  int tail = ring_buffer->tail;
  ring_buffer->data[tail] = ch;
  rx_timestamps(ring_buffer)[tail] = timestamp_us;
  ring_buffer->tail = (tail + 1) & ring_buffer->mask;
  return true;
}

/* Called by ports that do not provide a platform-specific timestamp. */
bool
UART_pushBuffer(RingBuffer *ring_buffer, uint8_t ch)
{
  return UART_pushBufferAt(ring_buffer, ch, (uint32_t)Machine_uptime_us());
}

bool
UART_popBuffer(RingBuffer *ring_buffer, uint8_t *ch)
{
  int head = ring_buffer->head;
  if (ring_buffer->tail == head) {
    return false;
  }
  *ch = ring_buffer->data[head];
  uart_rx_metadata *metadata = rx_metadata(ring_buffer);
  metadata->last_read_timestamp_us = rx_timestamps(ring_buffer)[head];
  metadata->last_read_timestamp_valid = true;
  ring_buffer->head = (head + 1) & ring_buffer->mask;
  return true;
}

bool
UART_unshiftBuffer(RingBuffer *ring_buffer, uint8_t ch)
{
  if (RingBuffer_free_size(ring_buffer) <= 1) {
    return false;
  }
  int head = (ring_buffer->head - 1) & ring_buffer->mask;
  ring_buffer->data[head] = ch;
  rx_timestamps(ring_buffer)[head] = (uint32_t)Machine_uptime_us();
  ring_buffer->head = head;
  return true;
}

bool
UART_lastReadTimestamp(RingBuffer *ring_buffer, uint64_t *timestamp_us)
{
  uart_rx_metadata *metadata = rx_metadata(ring_buffer);
  if (!metadata->last_read_timestamp_valid) {
    return false;
  }
  uint64_t now = Machine_uptime_us();
  uint32_t age = (uint32_t)now - metadata->last_read_timestamp_us;
  *timestamp_us = now - age;
  return true;
}

uint32_t
UART_rxOverflowCount(RingBuffer *ring_buffer)
{
  return rx_metadata(ring_buffer)->overflow_count;
}

int
UART_resolve_unit_num(const char *unit_name, int txd_pin, int rxd_pin)
{
#if defined(PICORB_PLATFORM_RP2)
  return UART_unit_num_from_pins(unit_name, txd_pin, rxd_pin);
#else
  (void)txd_pin;
  (void)rxd_pin;
  return UART_unit_name_to_unit_num(unit_name);
#endif
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/uart.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/uart.c"

#endif

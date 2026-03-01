/*
 * ringbuffer.h - Header-only lock-free ring buffer for single-producer /
 *                single-consumer use (e.g. ISR -> main thread).
 *
 * Usage:
 *   Allocate sizeof(RingBuffer) + size bytes, then call RingBuffer_init().
 *   `size` MUST be a power of two.
 */

#ifndef RINGBUFFER_H_
#define RINGBUFFER_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
  int head;
  int tail;
  size_t size;
  int mask;
  uint8_t data[];
} RingBuffer;

static inline bool
RingBuffer_init(RingBuffer *rb, size_t size)
{
  /* Verify size is a power of two */
  if (size == 0 || (size & (size - 1)) != 0) {
    return false;
  }
  rb->head = 0;
  rb->tail = 0;
  rb->size = size;
  rb->mask = (int)(size - 1);
  return true;
}

static inline size_t
RingBuffer_data_size(RingBuffer *rb)
{
  return (size_t)((rb->tail - rb->head) & rb->mask);
}

static inline size_t
RingBuffer_free_size(RingBuffer *rb)
{
  return rb->size - RingBuffer_data_size(rb);
}

static inline void
RingBuffer_clear(RingBuffer *rb)
{
  rb->head = 0;
  rb->tail = 0;
}

static inline bool
RingBuffer_push(RingBuffer *rb, uint8_t ch)
{
  if (RingBuffer_free_size(rb) < 1) {
    return false;
  }
  rb->data[rb->tail] = ch;
  rb->tail = (rb->tail + 1) & rb->mask;
  return true;
}

static inline bool
RingBuffer_pop(RingBuffer *rb, uint8_t *out)
{
  if (rb->tail == rb->head) {
    return false;
  }
  *out = rb->data[rb->head];
  rb->head = (rb->head + 1) & rb->mask;
  return true;
}

static inline bool
RingBuffer_pop_n(RingBuffer *rb, uint8_t *out, size_t len)
{
  if (len > rb->size) {
    return false;
  }
  if (rb->tail == rb->head) {
    return false;
  }
  size_t remaining = RingBuffer_data_size(rb);
  if (remaining < len) {
    len = remaining;
  }
  for (size_t i = 0; i < len; i++) {
    out[i] = rb->data[rb->head];
    rb->head = (rb->head + 1) & rb->mask;
  }
  return true;
}

static inline int
RingBuffer_search_char(RingBuffer *rb, uint8_t c)
{
  size_t count = RingBuffer_data_size(rb);
  for (size_t i = 0; i < count; i++) {
    if (rb->data[(rb->head + i) & rb->mask] == c) {
      return (int)i;
    }
  }
  return -1;
}

#endif /* RINGBUFFER_H_ */

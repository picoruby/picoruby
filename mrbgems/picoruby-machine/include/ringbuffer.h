/*
 * ringbuffer.h - Header-only lock-free ring buffer for single-producer /
 *                single-consumer use (e.g. ISR -> main thread).
 *
 * Usage:
 *   Allocate sizeof(RingBuffer) + size bytes, then call RingBuffer_init().
 *   `size` MUST be a power of two.
 *
 * Ownership: the producer owns `tail` and the consumer owns `head`.
 * Each side reads its own index plainly and the other side's index with
 * an acquire load, and publishes its own with a release store. That is
 * what makes the data write land before the index that exposes it, and
 * it holds whether the two sides are an ISR and the main thread on one
 * core or two threads on two cores.
 *
 * The build is -std=gnu99, so these are the GCC __atomic builtins rather
 * than C11 _Atomic. They are not free: on ARMv8-M (Cortex-M33, RP2350)
 * they become single lda/stl instructions, but on ARMv6-M (Cortex-M0+,
 * RP2040) gcc emits a dmb ish for each one. Push and pop therefore cost
 * two barriers each there, which is why the operations below read their
 * own index plainly instead of going through the two-sided
 * RingBuffer_data_size().
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

static inline int
RingBuffer_load_index(const int *index)
{
  return __atomic_load_n(index, __ATOMIC_ACQUIRE);
}

static inline void
RingBuffer_store_index(int *index, int value)
{
  __atomic_store_n(index, value, __ATOMIC_RELEASE);
}

/* Called before either side is running, so plain stores are enough. */
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

/*
 * Consumer-side occupancy, given the head it already holds. Only the
 * producer's tail has to be acquired.
 */
static inline size_t
RingBuffer_data_size_at(RingBuffer *rb, int head)
{
  return (size_t)((RingBuffer_load_index(&rb->tail) - head) & rb->mask);
}

/*
 * Producer-side room, given the tail it already holds. The mirror of the
 * above: only the consumer's head has to be acquired.
 */
static inline size_t
RingBuffer_free_size_at(RingBuffer *rb, int tail)
{
  return rb->size - (size_t)((tail - RingBuffer_load_index(&rb->head)) & rb->mask);
}

/*
 * Callable from either side, so both indices are loaded atomically. A
 * caller that knows which side it is on should use the _at form above
 * and save a barrier.
 */
static inline size_t
RingBuffer_data_size(RingBuffer *rb)
{
  return RingBuffer_data_size_at(rb, RingBuffer_load_index(&rb->head));
}

static inline size_t
RingBuffer_free_size(RingBuffer *rb)
{
  return rb->size - RingBuffer_data_size(rb);
}

/*
 * Consumer side. Discards everything the producer has published so far
 * by advancing `head` to it; writing the producer-owned `tail` from here
 * would break the ownership rule above.
 */
static inline void
RingBuffer_clear(RingBuffer *rb)
{
  RingBuffer_store_index(&rb->head, RingBuffer_load_index(&rb->tail));
}

/* Producer side. */
static inline bool
RingBuffer_push(RingBuffer *rb, uint8_t ch)
{
  int tail = rb->tail;
  if (RingBuffer_free_size_at(rb, tail) <= 1) {
    return false;
  }
  rb->data[tail] = ch;
  RingBuffer_store_index(&rb->tail, (tail + 1) & rb->mask);
  return true;
}

/* Consumer side. */
static inline bool
RingBuffer_pop(RingBuffer *rb, uint8_t *out)
{
  int head = rb->head;
  if (RingBuffer_load_index(&rb->tail) == head) {
    return false;
  }
  *out = rb->data[head];
  RingBuffer_store_index(&rb->head, (head + 1) & rb->mask);
  return true;
}

/*
 * Consumer side, and safe ONLY while the producer is stopped: moving
 * `head` backwards hands a slot the producer may already have taken back
 * to the consumer, and no ordering of the indices prevents that. A
 * consumer that needs pushback while the producer runs must keep it in
 * storage of its own.
 */
static inline bool
RingBuffer_unshift(RingBuffer *rb, uint8_t ch)
{
  int head;
  if (rb->size - RingBuffer_data_size_at(rb, rb->head) <= 1) {
    return false;
  }
  head = (rb->head - 1) & rb->mask;
  rb->data[head] = ch;
  RingBuffer_store_index(&rb->head, head);
  return true;
}

/* Consumer side. Publishes `head` once, after the whole copy. */
static inline bool
RingBuffer_pop_n(RingBuffer *rb, uint8_t *out, size_t len)
{
  int head = rb->head;
  size_t remaining;

  if (len > rb->size) {
    return false;
  }
  remaining = RingBuffer_data_size_at(rb, head);
  if (remaining == 0) {
    return false;
  }
  if (remaining < len) {
    len = remaining;
  }
  for (size_t i = 0; i < len; i++) {
    out[i] = rb->data[(head + (int)i) & rb->mask];
  }
  RingBuffer_store_index(&rb->head, (head + (int)len) & rb->mask);
  return true;
}

/* Consumer side. Returns the offset from `head`, or -1 if not found. */
static inline int
RingBuffer_search_char(RingBuffer *rb, uint8_t c)
{
  int head = rb->head;
  size_t count = RingBuffer_data_size_at(rb, head);
  for (size_t i = 0; i < count; i++) {
    if (rb->data[(head + (int)i) & rb->mask] == c) {
      return (int)i;
    }
  }
  return -1;
}

#endif /* RINGBUFFER_H_ */

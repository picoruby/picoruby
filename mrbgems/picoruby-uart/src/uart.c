#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "../include/uart.h"
#include "machine.h"
#include "irq.h"

/*
 * Whether RX publishes to the event bridge. This has to stay exactly
 * the condition mrbgem.rake uses to depend on picoruby-irq, or the two
 * disagree and the build breaks one way or the other.
 *
 * PICORB_IRQ_EVENT_BRIDGE alone is not that condition. Every mruby
 * build defines MRB_USE_TASK_SCHEDULER (lib/picoruby/build.rb), so the
 * bridge macro is on for ESP32 too -- where the gem is deliberately not
 * a dependency and IRQ_signal_from_isr would simply fail to link.
 */
#if defined(PICORB_IRQ_EVENT_BRIDGE) && !defined(PICORB_PLATFORM_ESP32)
#define PICORB_UART_EVENT_BRIDGE 1
#endif

#if defined(PICORB_ALLOC_ESTALLOC)
#include "picorb_heap.h"
#endif

#ifndef PICORB_UART_RX_BUFFER_SIZE
#define PICORB_UART_RX_BUFFER_SIZE (1 << 8)
#endif

#if PICORB_UART_RX_BUFFER_SIZE <= 0 || \
    (PICORB_UART_RX_BUFFER_SIZE & (PICORB_UART_RX_BUFFER_SIZE - 1)) != 0
#error "PICORB_UART_RX_BUFFER_SIZE must be a positive power of two"
#endif

typedef struct {
  uint32_t last_read_timestamp_us;
  uint32_t overflow_count;    /* producer writes, consumer reads */
  /*
   * One byte of pushback for ungetbyte, owned by the consumer alone.
   * It cannot live in the ring: unshifting there moves head backwards
   * into a slot the producer may already have taken back, and no
   * ordering of the indices prevents the two writes from colliding.
   * Depth one is all ungetbyte needs for a single token of lookahead.
   */
  uint32_t pushback_timestamp_us;
  uint8_t pushback_byte;
  bool pushback_valid;
  bool last_read_timestamp_valid;
} uart_rx_metadata;

/*
 * overflow_count crosses the same boundary as the ring indices: the
 * producer is an ISR or, on ESP32, a task on another core. There is
 * only one writer, so it needs no read-modify-write -- the producer
 * reads its own value plainly and republishes it. Relaxed is enough
 * because the counter orders nothing else, and __atomic_fetch_add would
 * be an out-of-line call to __atomic_fetch_add_4 on Cortex-M0+.
 */
static inline void
overflow_count_bump(uart_rx_metadata *metadata)
{
  __atomic_store_n(&metadata->overflow_count,
                   metadata->overflow_count + 1, __ATOMIC_RELAXED);
}

static inline uint32_t
overflow_count_read(const uart_rx_metadata *metadata)
{
  return __atomic_load_n(&metadata->overflow_count, __ATOMIC_RELAXED);
}

/*
 * The RX ring belongs to the unit, not to any Ruby object. The port's
 * ISR holds a raw pointer to it, so a ring owned by a Ruby object would
 * be freed under the ISR the moment that object was collected.
 *
 * Lifetime: a ring lives exactly as long as the allocator it came from.
 * mrb_open_with_custom_alloc() re-initialises the heap on every open
 * (picoruby-machine/src/mruby/alloc.c), so this table is only valid for
 * the lifetime of a single allocator. PicoRuby brings its allocator up
 * once at boot, never re-initialises it, and never opens a second VM
 * after closing the first, which is why nothing here is ever freed.
 * Anything that changes that MUST stop the producers and zero this
 * table first.
 *
 * Concurrency: `rx` is published before the port starts the producer
 * and never changes afterwards, so the ISR needs no synchronisation to
 * read it. Every consumer-side accessor below runs in VM context, where
 * C method bodies are serialized -- two Ruby UART objects on one unit
 * are therefore still one consumer, and the ring stays SPSC.
 */
typedef struct {
  RingBuffer *rx;
  size_t capacity;
} uart_unit_t;

static uart_unit_t units_[UART_UNIT_MAX];

static void *
uart_alloc(size_t size)
{
#if defined(PICORB_ALLOC_ESTALLOC)
  if (picorb_heap_ready()) {
    return picorb_heap_malloc(size);
  }
#endif
  return malloc(size);
}

static void
uart_free(void *ptr)
{
#if defined(PICORB_ALLOC_ESTALLOC)
  if (picorb_heap_ready()) {
    picorb_heap_free(ptr);
    return;
  }
#endif
  free(ptr);
}

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

static bool
valid_unit(int unit_num)
{
  return 0 <= unit_num && unit_num < UART_UNIT_MAX;
}

static RingBuffer *
unit_rx(int unit_num)
{
  return valid_unit(unit_num) ? units_[unit_num].rx : NULL;
}

RingBuffer *
UART_unit_rx(int unit_num)
{
  return unit_rx(unit_num);
}

int
UART_unit_open(int unit_num, size_t capacity)
{
  RingBuffer *rx;
  size_t allocation_size;

  if (!valid_unit(unit_num)) {
    return UART_ERROR_INVALID_UNIT;
  }
  if (capacity == 0) {
    capacity = PICORB_UART_RX_BUFFER_SIZE;
  }
  allocation_size = UART_rx_buffer_allocation_size(capacity);
  if (allocation_size == 0) {
    return UART_ERROR_BUFFER_SIZE;
  }
  if (units_[unit_num].rx) {
    /* Reopening reuses the ring, so bytes that already arrived are
       still there. Resizing would mean freeing a buffer the producer
       may be writing into, and on ESP32 that producer is a task on
       another core with no point at which it is provably idle. */
    return units_[unit_num].capacity == capacity
             ? UART_ERROR_NONE : UART_ERROR_BUFFER_INUSE;
  }
  rx = (RingBuffer *)uart_alloc(allocation_size);
  if (rx == NULL) {
    return UART_ERROR_NOMEM;
  }
  if (!UART_rx_buffer_init(rx, capacity)) {
    uart_free(rx);
    return UART_ERROR_BUFFER_SIZE;
  }
  units_[unit_num].capacity = capacity;
  units_[unit_num].rx = rx;
  return UART_ERROR_NONE;
}

bool
UART_pushBufferAt(RingBuffer *ring_buffer, uint8_t ch, uint32_t timestamp_us)
{
  uart_rx_metadata *metadata = rx_metadata(ring_buffer);
  int tail = ring_buffer->tail;
  if (RingBuffer_free_size_at(ring_buffer, tail) <= 1) {
    overflow_count_bump(metadata);
    return false;
  }
  ring_buffer->data[tail] = ch;
  rx_timestamps(ring_buffer)[tail] = timestamp_us;
  RingBuffer_store_index(&ring_buffer->tail, (tail + 1) & ring_buffer->mask);
  return true;
}

/* Called by ports that do not provide a platform-specific timestamp. */
bool
UART_pushBuffer(RingBuffer *ring_buffer, uint8_t ch)
{
  return UART_pushBufferAt(ring_buffer, ch, (uint32_t)Machine_uptime_us());
}

int
UART_event_source(int unit_num)
{
#if defined(PICORB_UART_EVENT_BRIDGE)
  switch (unit_num) {
    case 0:  return IRQ_SRC_UART0;
    case 1:  return IRQ_SRC_UART1;
    default: return -1;   /* a chip with more units than we have ids */
  }
#else
  (void)unit_num;
  return -1;
#endif
}

void
UART_signal_rx(int unit_num)
{
#if defined(PICORB_UART_EVENT_BRIDGE)
  /* An invalid id is a no-op on the ISR side, so an unmapped unit needs
     no check of its own here. */
  IRQ_signal_from_isr(UART_event_source(unit_num), 1);
#else
  (void)unit_num;
#endif
}

#if defined(PICORB_PLATFORM_POSIX)
size_t
UART_inject_rx(int unit_num, const uint8_t *src, size_t len)
{
  RingBuffer *rx = unit_rx(unit_num);
  size_t stored = 0;

  if (rx == NULL) {
    return 0;
  }
  while (stored < len) {
    if (!UART_pushBufferAt(rx, src[stored], (uint32_t)Machine_uptime_us())) {
      break;   /* ring full; the overflow counter has been bumped */
    }
    stored++;
  }
  /* Same rule as the real producer: bytes that were dropped are not
     something new to drain, so they do not signal. */
  if (0 < stored) {
    UART_signal_rx(unit_num);
  }
  return stored;
}
#endif /* PICORB_PLATFORM_POSIX */

static bool
pop_buffer(RingBuffer *ring_buffer, uint8_t *ch)
{
  int head = ring_buffer->head;
  uart_rx_metadata *metadata;

  if (RingBuffer_data_size_at(ring_buffer, head) == 0) {
    return false;
  }
  *ch = ring_buffer->data[head];
  metadata = rx_metadata(ring_buffer);
  metadata->last_read_timestamp_us = rx_timestamps(ring_buffer)[head];
  metadata->last_read_timestamp_valid = true;
  RingBuffer_store_index(&ring_buffer->head, (head + 1) & ring_buffer->mask);
  return true;
}

size_t
UART_bytes_available(int unit_num)
{
  RingBuffer *rx = unit_rx(unit_num);
  if (rx == NULL) {
    return 0;
  }
  return RingBuffer_data_size(rx) + (rx_metadata(rx)->pushback_valid ? 1 : 0);
}

bool
UART_getbyte(int unit_num, uint8_t *ch)
{
  RingBuffer *rx = unit_rx(unit_num);
  uart_rx_metadata *metadata;

  if (rx == NULL) {
    return false;
  }
  metadata = rx_metadata(rx);
  if (metadata->pushback_valid) {
    *ch = metadata->pushback_byte;
    metadata->pushback_valid = false;
    metadata->last_read_timestamp_us = metadata->pushback_timestamp_us;
    metadata->last_read_timestamp_valid = true;
    return true;
  }
  return pop_buffer(rx, ch);
}

/*
 * Consumes up to len bytes and returns how many were taken. It does NOT
 * touch last_read_timestamp_us, which is documented as the timestamp of
 * the byte most recently returned by getbyte; implementing this as a
 * loop over UART_getbyte would silently widen that contract.
 */
size_t
UART_read(int unit_num, uint8_t *dst, size_t len)
{
  RingBuffer *rx = unit_rx(unit_num);
  uart_rx_metadata *metadata;
  size_t taken = 0;
  size_t available;

  if (rx == NULL || len == 0) {
    return 0;
  }
  metadata = rx_metadata(rx);
  if (metadata->pushback_valid) {
    dst[0] = metadata->pushback_byte;
    metadata->pushback_valid = false;
    dst++;
    len--;
    taken = 1;
  }
  if (len == 0) {
    return taken;
  }
  available = RingBuffer_data_size(rx);
  if (available < len) {
    len = available;
  }
  if (0 < len) {
    RingBuffer_pop_n(rx, dst, len);
    taken += len;
  }
  return taken;
}

/*
 * Observes only: gets() searches first and consumes afterwards, so a
 * consuming search would lose the line it just found. The length counts
 * the pushback byte when there is one.
 */
int
UART_line_length(int unit_num)
{
  RingBuffer *rx = unit_rx(unit_num);
  uart_rx_metadata *metadata;
  int offset = 0;
  int pos;

  if (rx == NULL) {
    return -1;
  }
  metadata = rx_metadata(rx);
  if (metadata->pushback_valid) {
    if (metadata->pushback_byte == (uint8_t)'\n') {
      return 1;
    }
    offset = 1;
  }
  pos = RingBuffer_search_char(rx, (uint8_t)'\n');
  return pos < 0 ? -1 : pos + 1 + offset;
}

bool
UART_ungetbyte(int unit_num, uint8_t ch)
{
  RingBuffer *rx = unit_rx(unit_num);
  uart_rx_metadata *metadata;

  if (rx == NULL) {
    return false;
  }
  metadata = rx_metadata(rx);
  if (metadata->pushback_valid) {
    return false;   /* one byte of lookahead, and it is taken */
  }
  metadata->pushback_byte = ch;
  metadata->pushback_timestamp_us = (uint32_t)Machine_uptime_us();
  metadata->pushback_valid = true;
  return true;
}

/*
 * Discards buffered input and the bookkeeping that describes it. The
 * last-read timestamp goes too: the ring now outlives the Ruby objects
 * that read from it, so this is the only way back to a known state, and
 * after a clear there is no longer a "byte most recently read from this
 * buffer" that the caller can reason about.
 */
void
UART_clear_rx(int unit_num)
{
  RingBuffer *rx = unit_rx(unit_num);
  if (rx == NULL) {
    return;
  }
  RingBuffer_clear(rx);
  rx_metadata(rx)->pushback_valid = false;
  rx_metadata(rx)->last_read_timestamp_valid = false;
  UART_clear_rx_buffer(unit_num);
}

bool
UART_lastReadTimestamp(int unit_num, uint64_t *timestamp_us)
{
  RingBuffer *rx = unit_rx(unit_num);
  uart_rx_metadata *metadata;
  uint64_t now;
  uint32_t age;

  if (rx == NULL) {
    return false;
  }
  metadata = rx_metadata(rx);
  if (!metadata->last_read_timestamp_valid) {
    return false;
  }
  now = Machine_uptime_us();
  age = (uint32_t)now - metadata->last_read_timestamp_us;
  *timestamp_us = now - age;
  return true;
}

uint32_t
UART_rxOverflowCount(int unit_num)
{
  RingBuffer *rx = unit_rx(unit_num);
  return rx ? overflow_count_read(rx_metadata(rx)) : 0;
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

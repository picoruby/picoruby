#ifndef PSG_RINGBUF_H
#define PSG_RINGBUF_H
/*
 * Lock‑free single‑producer / single‑consumer ring buffer for PSG packets
 *   ‑ queue length   : 512 slots (configurable via BITS macro)
 *   ‑ producer (core‑0) : Ruby / Sequencer / LiveMixer
 *   ‑ consumer (core‑1) : ISR side of PSG engine
 *
 * Each slot carries an entire PSG command packet (up to 64‑bit).
 * Exactly one writer and one reader are assumed — no other locking required.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* 現時点では REG_WRITE だけ実装。拡張しやすいよう opcode を残しておく */
typedef enum {
    PSG_PKT_REG_WRITE = 0,
    PSG_PKT_LFO_SET   = 1,
    /* 1: LFO_SET, 2: MUTE_CH, … 追加予定 */
} psg_opcode_t;

/* ----- packet layout -------------------------------------------------- */
/* 8-byte slot, naturally aligned to 4 bytes */
typedef struct __attribute__((packed, aligned(4))) {
    uint16_t tick;    /* 0–65535 ms : 相対でも絶対でも可 */
    uint8_t  op;      /* psg_opcode_t */
    uint8_t  reg;     /* 0–13 or channel idx for LFO_SET … */
    uint8_t  val;     /* 主パラメータ (データ or rate_lo) */
    uint8_t  arg;     /* 副パラメータ (rate_hi/depth/shape …) */
} psg_packet_t;

#ifndef PSG_PACKET_QUEUE_BITS
#define PSG_PACKET_QUEUE_BITS 9 /* 2^9 = 512 */
#endif

#define PSG_PACKET_QUEUE_LEN  (1u << PSG_PACKET_QUEUE_BITS)
#define PSG_PACKET_QUEUE_MASK (PSG_PACKET_QUEUE_LEN - 1)

#ifndef PSG_COMPILER_BARRIER
#  if defined(__GNUC__) || defined(__clang__)
#    define PSG_COMPILER_BARRIER() __asm volatile("" ::: "memory")
#  elif __STDC_VERSION__ >= 201112L
#    include <stdatomic.h>
#    define PSG_COMPILER_BARRIER() atomic_thread_fence(memory_order_seq_cst)
#  else
#    define PSG_COMPILER_BARRIER() /* nothing (best effort) */
#  endif
#endif

/* ----- ring buffer descriptor ---------------------------------------- */
typedef struct {
  volatile uint16_t head;            /* writer cursor (next free) */
  volatile uint16_t tail;            /* reader cursor (next valid) */
  psg_packet_t      buf[PSG_PACKET_QUEUE_LEN];
} psg_ringbuf_t;

/* ----- producer API --------------------------------------------------- */

/* push: returns true on success, false if queue full (packet dropped) */
static inline bool
psg_ringbuf_push(psg_ringbuf_t *rb, psg_packet_t p)
{
  uint16_t head = rb->head;
  uint16_t next = (head + 1) & PSG_PACKET_QUEUE_MASK;
  if (next == rb->tail) {
    /* overflow: queue full */
    return false;
  }
  rb->buf[head] = p;                     /* store payload */
  PSG_COMPILER_BARRIER();
  rb->head = next;                       /* publish */
  return true;
}

/* ----- consumer API --------------------------------------------------- */

/* pop: returns true if a packet was read into *out */
static inline bool
psg_ringbuf_pop(psg_ringbuf_t *rb, psg_packet_t *out)
{
  uint16_t tail = rb->tail;
  if (tail == rb->head) {
    return false; /* empty */
  }
  *out = rb->buf[tail];
  PSG_COMPILER_BARRIER();
  rb->tail = (tail + 1) & PSG_PACKET_QUEUE_MASK;
  return true;
}

/* utility */
static inline bool
psg_ringbuf_empty(const psg_ringbuf_t *rb)
{
  return rb->head == rb->tail;
}

static inline void
psg_ringbuf_clear(psg_ringbuf_t *rb)
{
  rb->head = rb->tail = 0;
}

#endif /* PSG_RINGBUF_H */


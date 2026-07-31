/*
 * Source table for the ISR-to-task event bridge.
 *
 * VM-agnostic: this file only manipulates plain C state. Turning a
 * ready source into a Task::Queue token is the VM glue's job (see
 * src/mruby/irq.c), because that step allocates and must run in VM
 * context.
 */

#include <stddef.h>
#include "../include/irq.h"

typedef struct {
  volatile uint32_t pending;  /* event bits, OR-ed by the ISR */
  bool enqueued;              /* a token is outstanding; VM context only */
} irq_source_t;

static irq_source_t sources_[IRQ_MAX_SOURCES];
static volatile uint32_t ready_mask_;

/* `1 << id` would be undefined at id 31 (signed shift overflow). */
#define IRQ_BIT(id) (UINT32_C(1) << (id))

static inline bool
valid_id(int id)
{
  return 0 <= id && id < IRQ_MAX_SOURCES;
}

void
IRQ_signal_from_isr(int id, uint32_t bits)
{
  if (!valid_id(id)) return;
  /* Publish the bits before the ready flag: a consumer that sees the
     ready bit must also see everything the handler staged. */
  IRQ_hal_atomic_or_u32(&sources_[id].pending, bits);
  IRQ_hal_atomic_or_u32(&ready_mask_, IRQ_BIT(id));
}

uint32_t
IRQ_peek_ready(void)
{
  return IRQ_hal_atomic_load_u32(&ready_mask_);
}

void
IRQ_clear_ready(int id)
{
  if (!valid_id(id)) return;
  /* A signal that lands between the drain's read and this call is
     dropped from the ready set, but never lost: either a token is
     outstanding, in which case the bits it stands for are still in
     `pending` and the consumer will take them, or nothing is bound, in
     which case IRQ.bind re-asserts. */
  IRQ_hal_atomic_and_u32(&ready_mask_, ~IRQ_BIT(id));
}

bool
IRQ_any_pending(void)
{
  return IRQ_hal_atomic_load_u32(&ready_mask_) != 0;
}

bool
IRQ_mark_enqueued(int id)
{
  if (!valid_id(id) || sources_[id].enqueued) return false;
  sources_[id].enqueued = true;
  return true;
}

bool
IRQ_is_enqueued(int id)
{
  return valid_id(id) && sources_[id].enqueued;
}

uint32_t
IRQ_take_bits(int id)
{
  uint32_t bits;

  if (!valid_id(id)) return 0;
  bits = IRQ_hal_atomic_exchange_u32(&sources_[id].pending, 0);
  /* Releasing the token after claiming the bits is enough: a signal
     that lands before the exchange is in `bits`, and one that lands
     after it re-asserts the ready bit itself. Worst case the consumer
     sees one spurious token, which the contract allows. */
  sources_[id].enqueued = false;
  return bits;
}

void
IRQ_mark_ready(int id)
{
  if (!valid_id(id)) return;
  IRQ_hal_atomic_or_u32(&ready_mask_, IRQ_BIT(id));
}

uint32_t
IRQ_peek_pending(int id)
{
  if (!valid_id(id)) return 0;
  return IRQ_hal_atomic_load_u32(&sources_[id].pending);
}

void
IRQ_reset(void)
{
  int i;

  /* Interrupts may still be firing, so clear atomically. Events racing
     a VM boundary are dropped by design. */
  for (i = 0; i < IRQ_MAX_SOURCES; i++) {
    IRQ_hal_atomic_exchange_u32(&sources_[i].pending, 0);
    sources_[i].enqueued = false;
  }
  IRQ_hal_atomic_exchange_u32(&ready_mask_, 0);
}

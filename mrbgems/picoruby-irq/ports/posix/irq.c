/*
 * POSIX port: host test target for the event bridge.
 *
 * There is no interrupt source here -- tests drive the bridge with
 * IRQ.simulate, which runs in ordinary VM context -- so the GCC
 * __atomic builtins are all this port needs. The GPIO entry points
 * below exist so the gem links; GPIO interrupts are hardware-only.
 */

#include <stdint.h>
#include <stdbool.h>

#include "../../include/irq.h"

#if defined(PICORB_IRQ_EVENT_BRIDGE)

uint32_t
IRQ_hal_atomic_load_u32(const volatile uint32_t *p)
{
  return __atomic_load_n(p, __ATOMIC_SEQ_CST);
}

uint32_t
IRQ_hal_atomic_or_u32(volatile uint32_t *p, uint32_t bits)
{
  return __atomic_fetch_or(p, bits, __ATOMIC_SEQ_CST);
}

uint32_t
IRQ_hal_atomic_and_u32(volatile uint32_t *p, uint32_t mask)
{
  return __atomic_fetch_and(p, mask, __ATOMIC_SEQ_CST);
}

uint32_t
IRQ_hal_atomic_exchange_u32(volatile uint32_t *p, uint32_t v)
{
  return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST);
}

#endif /* PICORB_IRQ_EVENT_BRIDGE */

int
IRQ_register_gpio(int pin, int event_type, uint32_t debounce_ms)
{
  (void)pin; (void)event_type; (void)debounce_ms;
  return -1;
}

bool
IRQ_unregister_gpio(int irq_id)
{
  (void)irq_id;
  return false;
}

bool
IRQ_peek_event(int *irq_id, int *event_type)
{
  (void)irq_id; (void)event_type;
  return false;
}

void
IRQ_init(void)
{
}

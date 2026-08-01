#ifndef PICORUBY_IRQ_H_
#define PICORUBY_IRQ_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* IRQ management functions */
int IRQ_register_gpio(int pin, int event_type, uint32_t debounce_ms);
bool IRQ_unregister_gpio(int irq_id);
bool IRQ_peek_event(int *irq_id, int *event_type);
void IRQ_init(void);

/*
 * The event bridge needs Task::Queue and a scheduler hook. Both VMs
 * have them, but a build can be configured without: everything below --
 * including the source table an ISR would publish to -- compiles out,
 * so a build that has no consumer pays nothing.
 */
#if defined(PICORB_VM_MRUBY) && defined(MRB_USE_TASK_SCHEDULER)
#define PICORB_IRQ_EVENT_BRIDGE 1
#elif defined(PICORB_VM_MRUBYC) && defined(MRBC_TASK_SCHEDULER_HOOK)
#define PICORB_IRQ_EVENT_BRIDGE 1
#endif

#if defined(PICORB_IRQ_EVENT_BRIDGE)

/*
 * ISR-to-task event bridge
 *
 * An interrupt handler only stages work: it ORs event bits into a
 * source's `pending` word and marks the source ready. A drain running
 * in VM context later turns that into a token in a bound Task::Queue,
 * waking a task blocked in `pop`. Handlers never touch the VM.
 *
 * Source ids are compile-time constants -- fixed, process-lifetime,
 * never reused -- so no registration or generation counter is needed.
 */
/* Every source costs a pending word plus a binding slot, permanently,
 * on parts where RAM is the binding constraint -- a full 32 does not fit
 * on pico2_w. Raise it from a build config when a build really has that
 * many event producers; the bit arithmetic is written for up to 32. */
#ifndef IRQ_MAX_SOURCES
#define IRQ_MAX_SOURCES 8
#endif

/*
 * The one registry of source ids. Keeping them here rather than in each
 * driver's header is what makes a collision impossible to write.
 */
enum {
  IRQ_SRC_GPIO  = 0,  /* shared by the whole GPIO subsystem */
  IRQ_SRC_UART0 = 1,  /* one per unit: a reader of UART0 should not */
  IRQ_SRC_UART1 = 2,  /* wake for traffic on UART1 */
};

/* ISR side. Invalid ids are a no-op. Order is load-bearing: the bits
 * are published before the source is marked ready. */
void IRQ_signal_from_isr(int id, uint32_t bits);

/* VM side. A source stays ready until it has been dealt with, so a
 * drain that is cut short (the push allocates, and allocation can
 * raise) is simply retried at the next scheduler entry. */
uint32_t IRQ_peek_ready(void);        /* the ready set, unchanged */
void     IRQ_clear_ready(int id);     /* done with this source */
bool     IRQ_any_pending(void);       /* cheap check for the scheduler hook */
bool     IRQ_mark_enqueued(int id);   /* false if a token is already out */
bool     IRQ_is_enqueued(int id);
uint32_t IRQ_take_bits(int id);       /* claim bits, release the token */
void     IRQ_mark_ready(int id);      /* re-assert after a late bind */
uint32_t IRQ_peek_pending(int id);
void     IRQ_reset(void);             /* drop all state; used at VM boundaries */

/* Atomic primitives, provided by each port. Callable from ISR and task
 * context alike, sequentially consistent: the ISR publishes payload
 * (e.g. bytes in a driver ring) before the ready bit, and the consumer
 * must observe them in that order. */
uint32_t IRQ_hal_atomic_load_u32(const volatile uint32_t *p);
uint32_t IRQ_hal_atomic_or_u32(volatile uint32_t *p, uint32_t bits);
uint32_t IRQ_hal_atomic_and_u32(volatile uint32_t *p, uint32_t mask);
uint32_t IRQ_hal_atomic_exchange_u32(volatile uint32_t *p, uint32_t v);

#endif /* PICORB_IRQ_EVENT_BRIDGE */

typedef enum picorb_gpio_irq_event_t {
  PICORB_GPIO_IRQ_EVENT_LEVEL_LOW  = 1,
  PICORB_GPIO_IRQ_EVENT_LEVEL_HIGH = 2,
  PICORB_GPIO_IRQ_EVENT_EDGE_FALL  = 4,
  PICORB_GPIO_IRQ_EVENT_EDGE_RISE  = 8,
} picorb_gpio_irq_event_t;

#ifdef __cplusplus
}
#endif

#endif /* PICORUBY_IRQ_H_ */

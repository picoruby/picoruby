#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "hardware/gpio.h"
#include "hardware/irq.h"

#include "../../include/irq.h"

#define MAX_IRQ_HANDLERS 16
#define IRQ_EVENT_QUEUE_SIZE (1<<5)

typedef struct {
  int pin;
  uint32_t event_mask;
  bool enabled;
} mrb_irq_handler_t;

typedef struct {
  int irq_id;
  int event_type;
} irq_event_t;

static mrb_irq_handler_t irq_handlers[MAX_IRQ_HANDLERS];
static irq_event_t event_queue[IRQ_EVENT_QUEUE_SIZE];
static int queue_head = 0;
static int queue_tail = 0;
static int next_irq_id = 1;

static void
gpio_irq_callback(uint gpio, uint32_t events)
{
  /* Find handler for this GPIO pin and matching event */
  for (int i = 0; i < MAX_IRQ_HANDLERS; i++) {
    if (irq_handlers[i].enabled && irq_handlers[i].pin == gpio && (events & irq_handlers[i].event_mask)) {
      /* Add event to queue */
      int next_tail = (queue_tail + 1) & (IRQ_EVENT_QUEUE_SIZE - 1);
      if (next_tail != queue_head) {  /* Queue not full */
        event_queue[queue_tail].irq_id = i + 1;  /* IRQ ID is 1-based */
        event_queue[queue_tail].event_type = events;
        queue_tail = next_tail;
      }
      break;
    }
  }
}

int
IRQ_register_gpio(int pin, int event_type)
{
  /* Find free slot */
  int slot = -1;
  for (int i = 0; i < MAX_IRQ_HANDLERS; i++) {
    if (!irq_handlers[i].enabled) {
      slot = i;
      break;
    }
  }

  if (slot < 0) {
    return -1;  /* No free slots */
  }

  /* Convert event_type to GPIO event mask */
  uint32_t event_mask = 0;
  if (event_type & 1) event_mask |= GPIO_IRQ_LEVEL_LOW;   /* LEVEL_LOW = 1 */
  if (event_type & 2) event_mask |= GPIO_IRQ_LEVEL_HIGH;  /* LEVEL_HIGH = 2 */
  if (event_type & 4) event_mask |= GPIO_IRQ_EDGE_FALL;   /* EDGE_FALL = 4 */
  if (event_type & 8) event_mask |= GPIO_IRQ_EDGE_RISE;   /* EDGE_RISE = 8 */

  /* Configure GPIO IRQ */
  gpio_set_irq_enabled_with_callback(pin, event_mask, true, &gpio_irq_callback);

  /* Store handler info */
  irq_handlers[slot].pin = pin;
  irq_handlers[slot].event_mask = event_mask;
  irq_handlers[slot].enabled = true;

  return slot + 1;  /* Return 1-based ID */
}

bool
IRQ_unregister_gpio(int irq_id)
{
  int slot = irq_id - 1;  /* Convert to 0-based index */

  if (slot < 0 || slot >= MAX_IRQ_HANDLERS || !irq_handlers[slot].enabled) {
    return false;  /* Invalid ID or not registered */
  }

  bool prev_state = irq_handlers[slot].enabled;

  /* Disable GPIO IRQ */
  gpio_set_irq_enabled(irq_handlers[slot].pin, irq_handlers[slot].event_mask, false);

  /* Clear handler */
  memset(&irq_handlers[slot], 0, sizeof(mrb_irq_handler_t));

  return prev_state;
}

bool
IRQ_peek_event(int *irq_id, int *event_type)
{
  if (queue_head == queue_tail) {
    return false;  /* Queue empty */
  }

  *irq_id = event_queue[queue_head].irq_id;
  *event_type = event_queue[queue_head].event_type;

  queue_head = (queue_head + 1) & (IRQ_EVENT_QUEUE_SIZE - 1);

  return true;
}

void
IRQ_init(void)
{
  /* Initialize data structures */
  memset(irq_handlers, 0, sizeof(irq_handlers));
  memset(event_queue, 0, sizeof(event_queue));
  queue_head = 0;
  queue_tail = 0;
  next_irq_id = 1;
}

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

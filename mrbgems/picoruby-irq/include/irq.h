#ifndef PICORUBY_IRQ_H_
#define PICORUBY_IRQ_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* IRQ management functions */
int IRQ_register_gpio(int pin, int event_type);
bool IRQ_unregister_gpio(int irq_id);
bool IRQ_peek_event(int *irq_id, int *event_type);
void IRQ_init(void);

#ifdef __cplusplus
}
#endif

#endif /* PICORUBY_IRQ_H_ */

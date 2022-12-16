#ifndef PRK_ROTARY_ENCODER_DEFINED_H_
#define PRK_ROTARY_ENCODER_DEFINED_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void RotaryEncoder_gpio_init(uint32_t pin);
bool RotaryEncoder_gpio_get(uint32_t pin);
void RotaryEncoder_gpio_set_dir_in(uint32_t pin);
void RotaryEncoder_gpio_pull_up(uint32_t pin);
void RotaryEncoder_gpio_set_irq_enabled_with_callback(uint32_t pin, /* uint32_t event_mask, */ bool enabled, void *callback);

#ifdef __cplusplus
}
#endif

#endif /* PRK_ROTARY_ENCODER_DEFINED_H_ */



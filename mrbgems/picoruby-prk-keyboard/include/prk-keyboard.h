#ifndef PRK_KEYBOARD_DEFINED_H_
#define PRK_KEYBOARD_DEFINED_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NIL 0xFFFFFF

void Keyboard_uart_anchor_init(uint32_t pin);
void Keyboard_uart_partner_init(uint32_t pin);
uint8_t Keyboard_mutual_partner_get8_put24_blocking(uint32_t data24);
uint32_t Keyboard_mutual_anchor_put8_get24_nonblocking(uint8_t data);

#ifdef __cplusplus
}
#endif

#endif /* PRK_KEYBOARD_DEFINED_H_ */



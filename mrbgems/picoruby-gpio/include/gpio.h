#ifndef GPIO_DEFINED_H_
#define GPIO_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IN         0b000001
#define OUT        0b000010
#define HIGH_Z     0b000100
#define PULL_UP    0b001000
#define PULL_DOWN  0b010000
#define OPEN_DRAIN 0b100000

int GPIO_pin_num_from_char(const uint8_t *);

int GPIO_init(uint8_t);
void GPIO_set_dir(uint8_t, uint8_t);
void GPIO_set_pull(uint8_t, uint8_t);
void GPIO_set_open_drain(uint8_t);
int GPIO_read(uint8_t);
void GPIO_write(uint8_t, uint8_t);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_DEFINED_H_ */


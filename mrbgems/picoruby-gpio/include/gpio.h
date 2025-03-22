#ifndef GPIO_DEFINED_H_
#define GPIO_DEFINED_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IN         0b0000001
#define OUT        0b0000010
#define HIGH_Z     0b0000100
#define PULL_UP    0b0001000
#define PULL_DOWN  0b0010000
#define OPEN_DRAIN 0b0100000
#define ALT        0b1000000

int GPIO_pin_num_from_char(const uint8_t *);

void GPIO_init(uint8_t);
void GPIO_set_dir(uint8_t, uint8_t);
void GPIO_pull_up(uint8_t);
void GPIO_pull_down(uint8_t);
void GPIO_open_drain(uint8_t);
int GPIO_read(uint8_t);
void GPIO_write(uint8_t, uint8_t);
void GPIO_set_function(uint8_t, uint8_t);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_DEFINED_H_ */

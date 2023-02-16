#ifndef ADC_DEFINED_H_
#define ADC_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int ADC_pin_num_from_char(const uint8_t *);
int ADC_init(uint8_t);
uint16_t ADC_read(uint8_t);

#ifdef __cplusplus
}
#endif

#endif /* ADC_DEFINED_H_ */


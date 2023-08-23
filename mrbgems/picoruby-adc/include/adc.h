#ifndef ADC_DEFINED_H_
#define ADC_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if MRBC_USE_FLOAT == 1
typedef float mrbc_float_t;
#elif MRBC_USE_FLOAT == 2
typedef double mrbc_float_t;
#endif

int ADC_pin_num_from_char(const uint8_t *);
int ADC_init(uint8_t);
uint32_t ADC_read_raw(uint8_t);
#if MRBC_USE_FLOAT
mrbc_float_t ADC_read_voltage(uint8_t);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ADC_DEFINED_H_ */


#ifndef ADC_DEFINED_H_
#define ADC_DEFINED_H_

#include <stdint.h>
#include "picoruby.h"

#ifdef __cplusplus
extern "C" {
#endif


int ADC_pin_num_from_char(const uint8_t *);
int ADC_init(uint8_t);
uint32_t ADC_read_raw(uint8_t);
#ifndef PICORB_NO_FLOAT
picorb_float_t ADC_read_voltage(uint8_t);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ADC_DEFINED_H_ */


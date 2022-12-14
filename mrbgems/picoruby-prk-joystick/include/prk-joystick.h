#ifndef PRK_JOYSTICK_DEFINED_H_
#define PRK_JOYSTICK_DEFINED_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_ADC_COUNT  4

#define AXIS_INDEX_X   0
#define AXIS_INDEX_Y   1
#define AXIS_INDEX_Z   2
#define AXIS_INDEX_RZ  3
#define AXIS_INDEX_RX  4
#define AXIS_INDEX_RY  5

extern int8_t axes[MAX_ADC_COUNT];
extern uint8_t adc_offset[MAX_ADC_COUNT];
extern float sensitivity[MAX_ADC_COUNT];
extern int8_t drift_suppression;
extern int8_t drift_suppression_minus;

void Joystick_reset_zero_report(void);
void Joystick_adc_gpio_init(uint32_t gpio);
void Joystick_adc_select_input(uint32_t input);
uint16_t Joystick_adc_read(void);
void sleep_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* PRK_JOYSTICK_DEFINED_H_ */


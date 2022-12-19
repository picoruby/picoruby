#ifndef PRK_RGB_DEFINED_H_
#define PRK_RGB_DEFINED_H_

#include <stdint.h>
#include <mrubyc.h>

#ifdef __cplusplus
extern "C" {
#endif

extern mrbc_tcb *tcb_rgb;

uint32_t RGB_init_dma_ws2812(int channel, uint32_t leds_count, const volatile void *read_addr);
void RGB_init_pio(uint32_t pin, bool is_rgbw);
void RGB_dma_channel_set_read_addr(uint32_t channel, const volatile void *read_addr, bool trigger);

void RGB_init_sub(mrbc_class *mrbc_class_RGB);

#ifdef __cplusplus
}
#endif

#endif /* PRK_RGB_DEFINED_H_ */


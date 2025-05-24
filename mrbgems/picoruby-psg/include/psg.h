/*
 * picoruby-psg/include/psg.h
 */

#ifndef PSG_DEFINED_H_
#define PSG_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>
#include "psg_ringbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAMPLE_RATE       22050
#define CHIP_CLOCK        2000000   // 2 MHz
#define PWM_BITS          12
#define MAX_SAMPLE_WIDTH  ((1u << PWM_BITS) - 1)

void PSG_pwm_set_gpio_level(uint8_t gpio, uint16_t level);
void PSG_add_repeating_timer(void);
bool PSG_audio_cb(void );

// Ring buffer
void PSG_write_reg(uint8_t reg, uint8_t val);
bool PSG_rb_push(const psg_packet_t *p);
bool PSG_rb_peak(psg_packet_t *out);
void PSG_rb_pop(void);

// Cross core critical section
typedef uint32_t psg_cs_token_t;
psg_cs_token_t PSG_enter_critical(void);
void PSG_exit_critical(psg_cs_token_t token);

// Tick
extern volatile uint16_t g_tick_ms;
void PSG_tick_start_core1(uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif /* PSG_DEFINED_H_ */

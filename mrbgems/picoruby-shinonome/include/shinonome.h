#ifndef SHINONOME_DEFINED_H_
#define SHINONOME_DEFINED_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t *ascii_12;
uint8_t *ascii_16;
uint8_t *jis208_12maru;
uint8_t *jis208_16go;

typedef struct {
  uint16_t unicode;
  uint16_t jis;
} unicode2jis_t;
const unicode2jis_t *unicode2jis;

#ifdef __cplusplus
}
#endif

#endif /* SHINONOME_DEFINED_H_ */

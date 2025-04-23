#ifndef RMT_DEFINED_H_
#define RMT_DEFINED_H_

#include <stdint.h>

typedef struct RMT_symbol_dulation_t {
  uint32_t t0h_ns;
  uint32_t t0l_ns;
  uint32_t t1h_ns;
  uint32_t t1l_ns;
  uint32_t reset_ns;
} RMT_symbol_dulation_t;

int RMT_init(uint32_t gpio, RMT_symbol_dulation_t *rmt_symbol_dulation);
int RMT_write(uint8_t *buffer, uint32_t nbytes);

#endif /* RMT_DEFINED_H_ */

#include "../../include/rng.h"
#include "hardware/structs/rosc.h"
#include "pico/time.h"

uint8_t c_rng_random_byte_impl(void)
{
  uint32_t random = 0;
  uint32_t bit = 0;
  for (int i = 0; i < 8; i++) {
    while (true) {
      bit = rosc_hw->randombit;
      sleep_us(5);
      if (bit != rosc_hw->randombit)
        break;
    }
    random = (random << 1) | bit;
    sleep_us(5);
  }
  return (uint8_t) random;
}

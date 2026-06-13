#include "../../include/rng.h"
#include "pico/rand.h"

uint8_t
rng_random_byte_impl(void)
{
  uint32_t random = get_rand_32();
  return (uint8_t) random;
}

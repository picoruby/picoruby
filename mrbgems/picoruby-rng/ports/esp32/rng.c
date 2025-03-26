#include "../../include/rng.h"
#include "esp_random.h"

uint8_t
c_rng_random_byte_impl(void)
{
  uint32_t random = esp_random();
  return (uint8_t)random;
}

#include "pico/rand.h"

#include "../../include/picorb_random.h"

/*
 * Takes one byte from each get_rand_32(). pico_rand mixes new entropy into
 * its state on every call: the ROSC on RP2040, the TRNG on RP2350.
 */
int
picorb_random_bytes(void *buf, size_t len)
{
  unsigned char *p = (unsigned char *)buf;
  size_t i = 0;

  while (i < len) {
    p[i] = (unsigned char)get_rand_32();
    i++;
  }
  return 0;
}

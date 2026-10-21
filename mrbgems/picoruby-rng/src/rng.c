#include "../include/rng.h"

/*
 * Generic fill, weak so a port can replace it.
 *
 * A source that cannot fail (rp2040's get_rand_32, esp32's esp_random)
 * or that refuses to return a bad byte at all (posix, which exits) needs
 * nothing beyond the byte call, and always succeeds here. A port whose
 * source can fail -- a peripheral that may not raise its ready event --
 * defines this strongly and returns negative instead of filling the
 * buffer with whatever it has.
 */
__attribute__((weak)) int
rng_random_bytes_impl(uint8_t *buf, size_t len)
{
  for (size_t i = 0; i < len; i++) {
    buf[i] = rng_random_byte_impl();
  }
  return 0;
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/rng.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/rng.c"

#endif

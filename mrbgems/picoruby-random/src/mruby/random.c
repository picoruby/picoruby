#include "mruby.h"

/*
 * The mruby VM takes Random from mruby-random and Random.urandom from
 * mruby-random-urandom. On a board, Random.urandom reads the HAL below.
 * A POSIX build takes the posix port of mruby-random-urandom instead, so
 * this file gives no second definition there.
 */
#if !defined(PICORB_PLATFORM_POSIX)

#include "urandom_hal.h"

int
mrb_hal_urandom(mrb_state *mrb, void *buf, size_t len)
{
  (void)mrb;
  return picorb_random_bytes(buf, len);
}

#endif

void
mrb_picoruby_random_gem_init(mrb_state *mrb)
{
  (void)mrb;
}

void
mrb_picoruby_random_gem_final(mrb_state *mrb)
{
  (void)mrb;
}

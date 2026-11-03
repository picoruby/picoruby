#include "../../include/platform.h"
#include <string.h>

/*
 * Weak, as picoruby-machine's nRF52 port is for Machine_delay_ms: a
 * product that already defines Platform_name keeps winning, so this can
 * land without the product repository changing in the same step. Once
 * the product drops its own definition, this one takes over and
 * RUBY_PLATFORM becomes "NRF52" -- which is what Machine.posix? reads.
 */
__attribute__((weak)) void
Platform_name(char *buf, size_t size)
{
  (void)size;
  memcpy(buf, "NRF52", 6);
}

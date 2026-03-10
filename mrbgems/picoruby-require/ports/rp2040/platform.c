#include "../../include/platform.h"
#include <string.h>

void
Platform_name(char *buf, size_t size)
{
  (void)size;
#if defined(PICO_RP2040)
  memcpy(buf, "RP2040", 7);
#elif defined(PICO_RP2350)
  memcpy(buf, "RP2350", 7);
#endif
}

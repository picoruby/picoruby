#include <stdio.h>

#include "hal.h"

#ifdef MRBC_USE_HAL_POSIX
int
hal_getchar(void)
{
  return getchar();
}
#endif

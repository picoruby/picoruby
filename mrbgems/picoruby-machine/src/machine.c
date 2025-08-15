#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include "../include/machine.h"

#if !defined(PICORB_PLATFORM_POSIX)
#include "hal.h"
#endif

int
debug_printf(const char *format, ...)
{
  va_list args;
  va_start(args, format);
#if defined(PICORB_PLATFORM_POSIX)
  int len = vprintf(format, args);
#else
  char buffer[256];
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  hal_write(1, buffer, len);
  hal_flush(1);
#endif
  va_end(args);
  return len;
}


#if defined(PICORB_VM_MRUBY)

#include "mruby/machine.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/machine.c"

#endif

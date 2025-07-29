#include <stdarg.h>
#include <stdbool.h>
#include "../include/machine.h"
#include "../include/hal.h"

int
debug_printf(const char *format, ...)
{
  char buffer[256];
  va_list args;
  va_start(args, format);
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  hal_write(1, buffer, len);
  hal_flush(1);
  return len;
}


#if defined(PICORB_VM_MRUBY)

#include "mruby/machine.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/machine.c"

#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include "../include/machine.h"
#include "../include/hal.h"

#if !defined(PICORB_PLATFORM_POSIX)
#include "hal.h"
#endif

int
debug_printf(const char *format, ...)
{
  va_list args;
  va_start(args, format);
#if defined(PICORB_PLATFORM_POSIX)
  int len = vfprintf(stderr, format, args);
  vfprintf(stderr, "\n", args);
  len += 1;
#else
  char buffer[256];
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  picorb_hal_write(2, buffer, len);
  picorb_hal_write(2, "\r\n", 2);
  len += 2;
  picorb_hal_flush(2);
#endif
  va_end(args);
  return len;
}


#if defined(PICORB_VM_MRUBY)

#if defined(PICORB_ALLOC_ESTALLOC)
#include "mruby/alloc.c"
#endif
#include "mruby/machine.c"

#elif defined(PICORB_VM_MRUBYC)

#if defined(PICORB_ALLOC_ESTALLOC) && defined(MRBC_ALLOC_LIBC)
#include "mrubyc/alloc.c"
#endif
#include "mrubyc/machine.c"

#endif

#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/string.h"

void
mbedtls_debug(void *ctx, int level, const char *file, int line, const char *str)
{
  ((void) level);
  printf("%s:%04d: %s", file, line, str);
}


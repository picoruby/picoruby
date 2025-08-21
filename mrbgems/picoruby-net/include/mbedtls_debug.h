#ifndef MBEDTLS_DEBUG_DEFINED_H_
#define MBEDTLS_DEBUG_DEFINED_H_

#include "../../picoruby-mbedtls/lib/mbedtls/include/mbedtls/debug.h"

void mbedtls_debug(void *ctx, int level, const char *file, int line, const char *str);

#endif // MBEDTLS_DEBUG_DEFINED_H_

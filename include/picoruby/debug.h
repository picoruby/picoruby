#ifndef PICORUBY_DEBUG_H
#define PICORUBY_DEBUG_H

//#include "../mrbgems/picoruby-machine/include/machine.h"

#ifdef __cplusplus
extern "C" {
#endif

int debug_printf(const char *format, ...);

#if defined(PICORUBY_DEBUG)
#define D(...) debug_printf(__VA_ARGS__)
#else
#define D(...)
#endif

#ifdef __cplusplus
}
#endif

#endif

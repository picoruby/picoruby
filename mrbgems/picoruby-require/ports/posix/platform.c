#include "../../include/platform.h"
#include <sys/utsname.h>
#include <stdio.h>

void
Platform_name(char *buf, size_t size)
{
#if defined(__EMSCRIPTEN__)
  /* The platform the binary runs on, not the host's: with NODERAWFS,
     emscripten's uname answers Node's os.machine() and os.type(), such
     as "x86_64-Linux", and code that asks for "wasm" took the other path. */
# if defined(__wasm64__)
  snprintf(buf, size, "wasm64-Emscripten");
# else
  snprintf(buf, size, "wasm32-Emscripten");
# endif
#else
  struct utsname u;
  uname(&u);
  snprintf(buf, size, "%s-%s", u.machine, u.sysname);
#endif
}

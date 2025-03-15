#include <stdbool.h>
#include "../include/watchdog.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/watchdog.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/watchdog.c"

#endif

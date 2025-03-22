#include <stdbool.h>
#include "../include/cyw43.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/cyw43.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/cyw43.c"

#endif

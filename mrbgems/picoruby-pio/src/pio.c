#include <string.h>
#include "../include/pio.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/pio.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/pio.c"

#endif

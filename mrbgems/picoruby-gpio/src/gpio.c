#include "../include/gpio.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/gpio.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/gpio.c"

#endif

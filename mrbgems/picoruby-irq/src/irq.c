#include "../include/irq.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/irq.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/irq.c"

#endif


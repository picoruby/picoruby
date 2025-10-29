#include "../include/time-class.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/time.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/time.c"

#endif


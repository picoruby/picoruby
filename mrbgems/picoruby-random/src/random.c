#include "../include/picorb_random.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/random.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/random.c"

#endif

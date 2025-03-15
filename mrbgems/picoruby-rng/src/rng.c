#include "../include/rng.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/rng.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/rng.c"

#endif


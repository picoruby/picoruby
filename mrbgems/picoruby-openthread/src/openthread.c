#include "../include/openthread_glue.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/openthread.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/openthread.c"

#endif

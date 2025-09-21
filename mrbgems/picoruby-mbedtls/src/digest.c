#include "digest.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/digest.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/digest.c"

#endif

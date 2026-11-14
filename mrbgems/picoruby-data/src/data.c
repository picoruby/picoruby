#if defined(PICORB_VM_MRUBY)

#error "picoruby-data does not support the mruby VM. Use mruby-data instead."

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/data.c"

#endif

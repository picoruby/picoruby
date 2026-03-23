#if defined(PICORB_VM_MRUBY)

#error "picoruby-numeric-ext is not supported in mruby. Use mruby-numeric-ext"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/numeric_ext.c"

#endif

#if defined(PICORB_VM_MRUBY)

#include "mruby/regexp.c"

#elif defined(PICORB_VM_MRUBYC)

#error "mrubyc does not support JavaScript integration."

#endif

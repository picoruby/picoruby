#include <mruby_compiler.h>

#if defined(PICORB_VM_MRUBY)

#include "mruby/eval.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/eval.c"

#endif

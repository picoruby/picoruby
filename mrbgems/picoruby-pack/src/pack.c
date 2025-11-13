#include "../include/pack.h"

#if defined(PICORB_VM_MRUBY)
  #include "mruby/pack.c"
#elif defined(PICORB_VM_MRUBYC)
  #include "mrubyc/pack.c"
#endif

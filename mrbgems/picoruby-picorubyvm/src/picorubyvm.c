#include "../include/instruction_sequence.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/picorubyvm.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/picorubyvm.c"

#endif

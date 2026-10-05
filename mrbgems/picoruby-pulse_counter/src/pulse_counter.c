#include "pulse_counter.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/pulse_counter.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/pulse_counter.c"

#endif

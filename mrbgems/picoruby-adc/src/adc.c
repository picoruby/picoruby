#include "../include/adc.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/adc.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/adc.c"

#endif

#include "../include/pwm.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/pwm.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/pwm.c"

#endif

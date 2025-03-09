#include <stdbool.h>
#include "../include/machine.h"


#if defined(PICORB_VM_MRUBY)

#include "mruby/machine.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/machine.c"

#endif

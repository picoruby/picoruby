#include <string.h>
#include "../include/i2c.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/i2c.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/i2c.c"

#endif

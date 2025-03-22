#include <string.h>
#include "../include/spi.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/spi.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/spi.c"

#endif

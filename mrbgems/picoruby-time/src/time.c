#include <stdlib.h>
#include <stdio.h>
#include "../include/picogem_time.h"
#include "env.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/time.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/time.c"

#endif

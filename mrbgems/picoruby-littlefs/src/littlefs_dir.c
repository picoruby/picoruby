#include <stdio.h>
#include <stdint.h>

#include "../include/littlefs.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/littlefs_dir.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/littlefs_dir.c"

#endif

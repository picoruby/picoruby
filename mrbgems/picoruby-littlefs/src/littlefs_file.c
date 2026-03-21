#include <stdio.h>
#include <stdint.h>

#include "../include/littlefs.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/littlefs_file.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/littlefs_file.c"

#endif

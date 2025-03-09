#include <stdio.h>
#include <stdint.h>

#include "../include/fat.h"

#include "../lib/ff14b/source/ff.h"


#if defined(PICORB_VM_MRUBY)

#include "mruby/fat_dir.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/fat_dir.c"

#endif

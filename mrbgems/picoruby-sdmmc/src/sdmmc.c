#include <string.h>
#include "../include/sdmmc.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/sdmmc.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/sdmmc.c"

#endif

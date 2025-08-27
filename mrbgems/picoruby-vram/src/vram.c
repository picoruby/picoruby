#include "../include/vram.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/vram.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/vram.c"

#endif

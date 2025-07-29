#include "../include/pitchdetector.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/pitchdetector.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/pitchdetector.c"

#endif

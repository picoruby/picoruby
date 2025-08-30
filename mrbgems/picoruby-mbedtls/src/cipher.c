#include "cipher.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/cipher.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/cipher.c"

#endif

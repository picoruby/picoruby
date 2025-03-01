#include <stdlib.h>
#include <stdbool.h>
#include "../include/io-console.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/io-console.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/io-console.c"

#endif

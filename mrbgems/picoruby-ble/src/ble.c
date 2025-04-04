#include <stdbool.h>
#include "picoruby.h"
#include "../include/ble.h"
#include "../include/ble_central.h"

#include "../../picoruby-machine/include/machine.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/ble.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/ble.c"

#endif

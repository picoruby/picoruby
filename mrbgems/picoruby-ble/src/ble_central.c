#include <stdbool.h>
#include "../include/ble.h"
#include "../include/ble_central.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/ble_central.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/ble_central.c"

#endif

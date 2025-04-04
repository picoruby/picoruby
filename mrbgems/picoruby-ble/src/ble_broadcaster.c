#include <stdbool.h>
#include "../include/ble.h"
#include "../include/ble_peripheral.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/ble_broadcaster.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/ble_broadcaster.c"

#endif

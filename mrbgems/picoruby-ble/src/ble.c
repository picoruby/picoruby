#include <stdbool.h>
#include "picoruby.h"
#include "../include/ble.h"
#include "../include/ble_central.h"

#include "../../picoruby-machine/include/machine.h"

/*
 * Workaround: To avoid deadlock
 * TODO: Maybe we need a critical section instead of these simple mutex
 */
static bool write_values_mutex = false;

#if defined(PICORB_VM_MRUBY)

#include "mruby/ble.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/ble.c"

#endif

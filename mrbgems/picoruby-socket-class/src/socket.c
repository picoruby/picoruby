/*
 * Common socket logic
 *
 * This file switches between mruby and mruby/c VM bindings
 */

#include "../include/socket.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/socket.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/socket.c"

#endif

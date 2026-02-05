/*
 * Copyright (c) 2025 HASUMI Hitoshi | MIT License
 */

#include "../include/usb-hid.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/usb-hid.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/usb-hid.c"

#endif

#include "../include/socket.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/udp_socket.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/udp_socket.c"

#endif

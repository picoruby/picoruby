#include "../include/socket.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/ssl_socket.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/ssl_socket.c"

#endif

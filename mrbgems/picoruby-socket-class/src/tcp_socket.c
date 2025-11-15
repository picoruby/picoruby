#include "../include/socket.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/tcp_socket.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/tcp_socket.c"

#endif

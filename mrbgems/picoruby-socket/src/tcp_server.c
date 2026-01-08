#include "../include/socket.h"

#if defined(PICORB_VM_MRUBY)

#include "mruby/tcp_server.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/tcp_server.c"

#endif

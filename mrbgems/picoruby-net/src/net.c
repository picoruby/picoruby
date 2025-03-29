#include "../include/net.h"


#if defined(PICORB_VM_MRUBY)

#include "mruby/mbedtls_debug.c"
#include "mruby/net/common.c"
#include "mruby/net/dns.c"
#include "mruby/net/tcp.c"
#include "mruby/net/udp.c"
#include "mruby/net.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/mbedtls_debug.c"
#include "mrubyc/net/common.c"
#include "mrubyc/net/dns.c"
#include "mrubyc/net/tcp.c"
#include "mrubyc/net/udp.c"
#include "mrubyc/net.c"

#endif


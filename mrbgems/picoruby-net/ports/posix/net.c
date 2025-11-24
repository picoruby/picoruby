/*
 * POSIX Network Implementation
 * Main file that includes platform utilities and Ruby bindings
 */

#include <unistd.h>
#include "../include/net.h"

void
Net_sleep_ms(int ms)
{
  usleep(ms * 1000);
}

/* Include Ruby bindings based on VM type */
#if defined(PICORB_VM_MRUBY)

#include "../src/mruby/net.c"

#elif defined(PICORB_VM_MRUBYC)

#include "../src/mrubyc/net.c"

/*
 * Wrapper for gem initialization system compatibility
 * The gem_init.c expects mruby-style init functions, but mrubyc uses mrbc_net_init
 * This wrapper bridges the two systems
 */
void mrb_picoruby_net_gem_init(void *mrb) {
  /* mrubyc init is called separately through mrbc_net_init */
  /* This function exists to satisfy the gem_init.c linker requirements */
  (void)mrb;
}

void mrb_picoruby_net_gem_final(void *mrb) {
  (void)mrb;
}

#endif

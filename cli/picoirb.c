#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/mrubyc/src/mrubyc.h"

#if defined(PICORBC_DEBUG) && !defined(MRBC_ALLOC_LIBC)
  #include "../src/mrubyc/src/alloc.c"
#endif

#include "../src/picorbc.h"

#include "heap.h"

/* Ruby */
#include "ruby/buffer.c"
#include "ruby/irb.c"

#include "sandbox.h"

int loglevel;
static uint8_t heap[HEAP_SIZE];

void
c_getc(mrb_vm *vm, mrb_value *v, int argc)
{
  SET_INT_RETURN(getc(stdin));
}

int
main(int argc, char *argv[])
{
  loglevel = LOGLEVEL_WARN;
  mrbc_init(heap, HEAP_SIZE);
  mrbc_define_method(0, mrbc_class_object, "getc", c_getc);
  SANDBOX_INIT();
  create_sandbox();
  mrbc_create_task(buffer, 0);
  mrbc_create_task(irb, 0);
  mrbc_run();
  return 0;
}

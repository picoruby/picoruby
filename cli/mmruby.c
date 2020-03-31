#include <stdio.h>
#include <stdlib.h>
#include "../src/mrubyc/src/mrubyc.h"
#include "../src/common.h"
#include "../src/compiler.h"
#include "../src/debug.h"
#include "../src/scope.h"
#include "../src/stream.h"
#include "heap.h"

void run(uint8_t *mrb)
{
  init_static();
  struct VM *vm = mrbc_vm_open(NULL);
  if( vm == 0 ) {
    fprintf(stderr, "Error: Can't open VM.\n");
    return;
  }

  if( mrbc_load_mrb(vm, mrb) != 0 ) {
    fprintf(stderr, "Error: Illegal bytecode.\n");
    return;
  }

  mrbc_vm_begin(vm);
  mrbc_vm_run(vm);
  mrbc_vm_end(vm);
  mrbc_vm_close(vm);
}

static uint8_t heap[HEAP_SIZE];

int main(int argc, char *argv[])
{
  loglevel = LOGLEVEL_INFO;
  mrbc_init_alloc(heap, HEAP_SIZE);

  Scope *scope = Scope_new(NULL);
  StreamInterface *si = StreamInterface_new("puts \"Hello World!\"\n", STREAM_TYPE_MEMORY);

  if (Compile(scope, si)) {
    run(scope->vm_code);
  }

  StreamInterface_free(si);
  Scope_free(scope);
#ifdef MMRBC_DEBUG
  memcheck();
#endif
  return 0;
}

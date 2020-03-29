#include <stdio.h>
#include <stdlib.h>
#include "../src/mrubyc/src/mrubyc.h"
#include "../src/common.h"
#include "../src/debug.h"
#include "../src/fmemstream.h"
#include "heap.h"
/*
void mrubyc(uint8_t *mrbbuf)
{
  struct VM *vm;

  mrbc_init_alloc(heap, MEMORY_SIZE);
  init_static();

  vm = mrbc_vm_open(NULL);
  if( vm == 0 ) {
    fprintf(stderr, "Error: Can't open VM.\n");
    return;
  }

  if( mrbc_load_mrb(vm, mrbbuf) != 0 ) {
    fprintf(stderr, "Error: Illegal bytecode.\n");
    return;
  }

  mrbc_vm_begin(vm);
  mrbc_vm_run(vm);
  mrbc_vm_end(vm);
  mrbc_vm_close(vm);
}
*/

static uint8_t heap[HEAP_SIZE];

int main(int argc, char *argv[])
{
  loglevel = LOGLEVEL_INFO;
  mrbc_init_alloc(heap, HEAP_SIZE);
  fmemstream *stream = fmemstreamopen("Helo\n1\nlkjijerg\ng");
  char *line = mmrbc_alloc(10);
  while (fmemeof((FILE *)stream) != 0) {
    if (fmemgets(line, 10, (FILE *)stream) != NULL) {
      WARN("%s", line);
    } else {
      ERROR("error\n");
    }
  }
  fmemstreamclose(stream);
  mmrbc_free(line);
  return 0;
}

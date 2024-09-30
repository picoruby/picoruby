#include <mrubyc.h>
#include <picogem_init.c>
#include <executables_init.c>

#define MRBC_MEMORY_SIZE (1024*1024)
static uint8_t heap[MRBC_MEMORY_SIZE];

int
main(void)
{
  mrbc_init(heap, MRBC_MEMORY_SIZE);
  mrbc_tcb *tcb = mrbc_create_task(sqlite3_test, 0);
  mrbc_vm *vm = &tcb->vm;
  picoruby_init_require(vm);
  picoruby_init_executables(vm);
  mrbc_run();
}


#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <mruby_compiler.h>
#include <picogem_init.c>
#include <executables_init.c>

#ifndef HEAP_SIZE
#define HEAP_SIZE (1024 * 2000)
#endif

//int loglevel = LOGLEVEL_FATAL; /* must be LOGLEVEL_FATAL so that irb works */
static uint8_t heap_pool[HEAP_SIZE];

static void
c_exit(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pid_t pid = getpid();
  kill(pid, SIGINT);
}

int
main(void)
{
  mrbc_init(heap_pool, HEAP_SIZE);
  mrbc_tcb *tcb = mrbc_create_task(r2p2, 0);
  mrbc_vm *vm = &tcb->vm;
  mrbc_define_method(vm, mrbc_class_object, "exit", c_exit);
  picoruby_init_require(vm);
  picoruby_init_executables(vm);
  mrbc_run();
  return 0;
}


#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "picoruby.h"
#include "mruby_compiler.h"

#include "app.c"

#if defined(PICORB_VM_MRUBYC)

#ifndef HEAP_SIZE
#define HEAP_SIZE (1024 * 2000)
#endif

static uint8_t heap_pool[HEAP_SIZE];

int
main(void)
{
  mrbc_init(heap_pool, HEAP_SIZE);
  mrbc_tcb *tcb = mrbc_create_task(app, 0);
  mrbc_vm *vm = &tcb->vm;
  picoruby_init_require(vm);
  mrbc_run();
  return 0;
}

#elif defined(PICORB_VM_MRUBY)

mrb_state *global_mrb = NULL;

int
main(void)
{
  int ret = 0;
  mrb_state *mrb = mrb_open();
  global_mrb = mrb;
  mrc_irep *irep = mrb_read_irep(mrb, app);
  mrc_ccontext *cc = mrc_ccontext_new(mrb);
  mrb_tcb *tcb = mrc_create_task(cc, irep, NULL);
  tcb->c.ci->stack[0] = mrb_obj_value(mrb->top_self);
  if (!tcb) {
    fprintf(stderr, "mrbc_create_task failed\n");
    ret = 1;
  }
  else {
    mrb_tasks_run(mrb);
  }
  mrb_close(mrb);
  mrc_ccontext_free(cc);
  return ret;
}

#endif

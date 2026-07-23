#include <stdio.h>
#include <stdlib.h>

#include "picoruby.h"
#include "mruby_compiler.h"

#include "app.c"

#ifndef HEAP_SIZE
#define HEAP_SIZE (1024 * 1000)
#endif

static uint8_t heap_pool[HEAP_SIZE] __attribute__((aligned(16)));

extern mrb_state *global_mrb; /* defined in mruby-compiler (ccontext.c) */

int
main(void)
{
  mrb_state *mrb = mrb_open_with_custom_alloc(heap_pool, HEAP_SIZE);
  if (mrb == NULL) {
    fprintf(stderr, "mrb_open failed\n");
    return EXIT_FAILURE;
  }
  global_mrb = mrb;

  int ret = 0;
  mrc_irep *irep = mrb_read_irep(mrb, app);
  mrc_ccontext *cc = mrc_ccontext_new(mrb);
  mrb_value name = mrb_str_new_cstr(mrb, "sqlite3-test");
  mrb_value task = mrc_create_task(cc, irep, name, mrb_nil_value(), mrb_obj_value(mrb->top_self));
  if (mrb_nil_p(task)) {
    fprintf(stderr, "mrc_create_task failed\n");
    ret = EXIT_FAILURE;
  }
  else {
    mrb_task_run(mrb);
  }
  mrb_close(mrb);
  mrc_ccontext_free(cc);
  return ret;
}

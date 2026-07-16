#ifndef PICORUBY_ESTALLOC_MRUBY_H
#define PICORUBY_ESTALLOC_MRUBY_H

#include <stddef.h>
#include <mruby.h>
#include "picorb_heap.h"

MRB_BEGIN_DECL

mrb_state *mrb_open_with_custom_alloc(void *mem, size_t bytes);
mrb_value mrb_alloc_statistics(mrb_state *mrb);
void mrb_alloc_set_critical_section(void (*enter)(void), void (*exit_func)(void));

MRB_END_DECL

#endif

#ifndef MRC_UTILS_H
#define MRC_UTILS_H

#include "picoruby.h"

MRB_BEGIN_DECL

mrb_state *mrb_open_with_custom_alloc(void* mem, size_t bytes);
mrb_value mrb_alloc_statistics(mrb_state *mrb);

#if defined(PICORB_ALLOC_ESTALLOC)
void mrb_alloc_set_critical_section(void (*enter)(void), void (*exit_func)(void));
#endif

MRB_END_DECL

#endif

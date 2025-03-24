#ifndef MRC_UTILS_H
#define MRC_UTILS_H

#include "picoruby.h"

MRB_BEGIN_DECL

mrb_state *mrb_open_with_custom_alloc(void* mem, size_t bytes);
mrb_value mrb_alloc_statistics(mrb_state *mrb);

MRB_END_DECL

#endif

#ifndef MRC_UTILS_H
#define MRC_UTILS_H

#include "picoruby.h"

MRB_BEGIN_DECL

mrb_value mrb_tlsf_statistics(mrb_state *mrb);
mrb_state *mrb_open_with_tlsf(void* mem, size_t bytes);

MRB_END_DECL

#endif

#ifndef MD_CONTEXT_DEFINED_H_
#define MD_CONTEXT_DEFINED_H_

#include "mruby/data.h"
#include "mruby/class.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct mrb_data_type mrb_md_context_type;

void mrb_md_context_free(mrb_state *mrb, void *ptr);

#ifdef __cplusplus
}
#endif

#endif

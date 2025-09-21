#ifndef MD_CONTEXT_DEFINED_H_
#define MD_CONTEXT_DEFINED_H_

#include "mruby/data.h"
#include "mruby/class.h"

#ifdef __cplusplus
extern "C" {
#endif

void MbedTLS_md_free(void *p);

static void
mrb_md_context_free(mrb_state *mrb, void *ptr)
{
  MbedTLS_md_free(ptr);
  mrb_free(mrb, ptr);
}

static struct mrb_data_type mrb_md_context_type = {
  "MdContext", mrb_md_context_free,
};

#ifdef __cplusplus
}
#endif

#endif


#ifndef TLSF_UTILS_H
#define TLSF_UTILS_H

#if defined(PICORB_VM_MRUBY)
#include "task.h"

void mrc_resolve_intern(mrc_ccontext *cc, mrc_irep *irep);
mrb_value mrc_create_task(mrc_ccontext *cc, mrc_irep *irep, mrb_value name, mrb_value priority, mrb_value top_self);

#endif

#endif // TLSF_UTILS_H

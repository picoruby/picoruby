#ifndef MRC_UTILS_H
#define MRC_UTILS_H

#if defined(PICORB_VM_MRUBY)
#include "task.h"

int mrc_string_run(mrc_ccontext *cc, const char *string);
void mrc_resolve_intern(mrc_ccontext *cc, mrc_irep *irep);
mrb_tcb *mrc_create_task(mrc_ccontext *cc, mrc_irep *irep, mrb_tcb *tcb, const char *name);
#endif

#endif

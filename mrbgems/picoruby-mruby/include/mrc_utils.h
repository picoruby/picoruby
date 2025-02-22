#ifndef MRC_UTILS_H
#define MRC_UTILS_H

#include "task.h"

MRB_BEGIN_DECL

int mrc_string_run(mrc_ccontext *cc, const char *string);
void mrc_resolve_intern(mrc_ccontext *cc, mrc_irep *irep);
mrb_tcb *mrc_create_task(mrc_ccontext *cc, mrc_irep *irep, mrb_tcb *tcb);

MRB_END_DECL

#endif

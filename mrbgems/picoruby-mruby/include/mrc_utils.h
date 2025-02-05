#ifndef MRC_UTILS_H
#define MRC_UTILS_H

void mrc_resolve_mrb_intern(mrc_ccontext *c, mrc_irep *irep);
int mrc_string_run_cxt(mrc_ccontext *c, const char *string);
int mrc_string_run(mrb_state *mrb, const char *string);

#endif

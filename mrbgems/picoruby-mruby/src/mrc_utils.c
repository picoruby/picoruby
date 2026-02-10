#include <picoruby.h>
#include <mruby/proc.h>
#include <mruby/data.h>
#include <task.h>

MRC_API void
mrc_resolve_intern(mrc_ccontext *cc, mrc_irep *irep)
{
  pm_constant_pool_t *constant_pool = &cc->p->constant_pool;
  if (constant_pool->constants == NULL) {
    return;
  }

  // Symbols
  if (0 < irep->slen) {
    picorb_sym *new_syms = (picorb_sym *)mrc_malloc(cc, sizeof(picorb_sym) *irep->slen);
    for (int i = 0; i < irep->slen; i++) {
      mrc_sym sym = irep->syms[i];
      pm_constant_t *constant = pm_constant_pool_id_to_constant(constant_pool, sym);
      const char *lit = (const char *)constant->start;
      size_t len = constant->length;
      new_syms[i] = mrb_intern(cc->mrb, lit, len);
    }
    mrc_free(cc, (mrc_sym *)irep->syms); // discard const
    irep->syms = new_syms;
  }

  // Local variables
  int lv_size = irep->nlocals - 1; // exclude self
  if (0 < lv_size) {
    picorb_sym *new_lv = (picorb_sym *)mrc_malloc(cc, sizeof(picorb_sym) * lv_size);
    for (int i = 0; i < lv_size; i++) {
      mrc_sym sym = irep->lv[i];
      pm_constant_t *constant = pm_constant_pool_id_to_constant(constant_pool, sym);
      const char *lit = (const char *)constant->start;
      size_t len = constant->length;
      new_lv[i] = mrb_intern(cc->mrb, lit, len);
    }
    mrc_free(cc, (mrc_sym *)irep->lv); // discard const
    irep->lv = new_lv;
  }

  for (int i = 0; i < irep->rlen; i++) {
    mrc_resolve_intern(cc, (mrc_irep *)irep->reps[i]);
  }
}

MRC_API mrb_value
mrc_create_task(mrc_ccontext *cc, mrc_irep *irep, mrb_value name, mrb_value priority, mrb_value top_self)
{
  mrc_resolve_intern(cc, irep);
  struct RProc *proc = mrb_proc_new(cc->mrb, (mrb_irep *)irep);
  proc->c = NULL;
  return mrb_create_task(cc->mrb, proc, name, priority, top_self);
}


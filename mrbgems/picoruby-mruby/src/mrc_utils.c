#include <picoruby.h>
#include <mruby/proc.h>

MRC_API void
mrb_picoruby_mruby_gem_init(mrb_state *mrb)
{
}

MRC_API void
mrb_picoruby_mruby_gem_final(mrb_state *mrb)
{
}

MRC_API void
mrc_resolve_intern(mrc_ccontext *cc, mrc_irep *irep)
{
  pm_constant_pool_t *constant_pool = &cc->p->constant_pool;
  picorb_sym *new_syms = mrc_malloc(cc, sizeof(picorb_sym) *irep->slen);
  for (int i = 0; i < irep->slen; i++) {
    mrc_sym sym = irep->syms[i];
    pm_constant_t *constant = pm_constant_pool_id_to_constant(constant_pool, sym);
    const char *lit = (const char *)constant->start;
    size_t len = constant->length;
    new_syms[i] = mrb_intern(cc->mrb, lit, len);
  }
  mrc_free(cc, (mrc_sym *)irep->syms); // discard const
  irep->syms = new_syms;
  for (int i = 0; i < irep->rlen; i++) {
    mrc_resolve_intern(cc, (mrc_irep *)irep->reps[i]);
  }
}

MRC_API int
mrc_string_run_cxt(mrc_ccontext *cc, const char *string)
{
  mrb_state *mrb = cc->mrb;
  const uint8_t *source = (const uint8_t *)string; mrc_irep *irep = mrc_load_string_cxt(cc, &source, (size_t)strlen((const char *)string));

  struct RClass *target = mrb->object_class;
  struct RProc *proc = mrb_proc_new(mrb, (mrb_irep *)irep);
  proc->c = NULL;
  if (mrb->c->cibase && mrb->c->cibase->proc == proc->upper) {
    proc->upper = NULL;
  }
  MRB_PROC_SET_TARGET_CLASS(proc, target);
  if (mrb->c->ci) {
    mrb_vm_ci_target_class_set(mrb->c->ci, target);
  }
  mrc_resolve_intern(cc, irep);
  mrb_top_run(mrb, proc, mrb_top_self(mrb), 1);
  mrb_vm_ci_env_clear(mrb, mrb->c->cibase);
  if (mrb->exc) {
    // TODO: MRB_EXC_CHECK_EXIT(mrb, mrb->exc) or something like that
    return EXIT_FAILURE;
  }
  if (irep) mrc_irep_free(cc, irep);
  return EXIT_SUCCESS;
}

MRC_API int
mrc_string_run(mrb_state *mrb, const char *string)
{
  mrc_ccontext *cc = mrc_ccontext_new(mrb);
  return mrc_string_run_cxt(cc, string);
}


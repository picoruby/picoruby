#include <picoruby.h>
#include <mruby/proc.h>
#include <mruby/array.h>
#include <mruby/data.h>
#include <task.h>
#include <mrc_debug.h>

// Interns a name for the irep tree being resolved, and keeps the symbol in
// `keep` until the whole tree is done. Until then no proc holds the tree, so
// symbol GC, which a later intern can start, would free the names already
// interned.
static picorb_sym
intern_kept(mrc_ccontext *cc, mrb_value keep, pm_constant_t *constant)
{
  mrb_sym sym = mrb_intern(cc->mrb, (const char *)constant->start, constant->length);
  mrb_ary_push(cc->mrb, keep, mrb_symbol_value(sym));
  return sym;
}

static void
resolve_intern(mrc_ccontext *cc, mrc_irep *irep, mrb_value keep)
{
  pm_constant_pool_t *constant_pool = &cc->p->constant_pool;

  // Symbols
  if (0 < irep->slen) {
    picorb_sym *new_syms = (picorb_sym *)mrc_malloc(cc, sizeof(picorb_sym) *irep->slen);
    for (int i = 0; i < irep->slen; i++) {
      mrc_sym sym = irep->syms[i];
      if (sym == 0) {
        new_syms[i] = 0;
        continue;
      }
      pm_constant_t *constant = pm_constant_pool_id_to_constant(constant_pool, sym);
      new_syms[i] = intern_kept(cc, keep, constant);
    }
    mrc_free(cc, (mrc_sym *)irep->syms); // discard const
    irep->syms = new_syms;
  }

  // Local variables (irep->lv is NULL after mrc_irep_remove_lv)
  int lv_size = irep->nlocals - 1; // exclude self
  if (0 < lv_size && irep->lv) {
    picorb_sym *new_lv = (picorb_sym *)mrc_malloc(cc, sizeof(picorb_sym) * lv_size);
    for (int i = 0; i < lv_size; i++) {
      mrc_sym sym = irep->lv[i];
      // Skip when lvar list has only anonymous block parameter (e.g. `|((*))|`)
      if (sym == 0) {
        new_lv[i] = 0;
        continue;
      }
      pm_constant_t *constant = pm_constant_pool_id_to_constant(constant_pool, sym);
      new_lv[i] = intern_kept(cc, keep, constant);
    }
    mrc_free(cc, (mrc_sym *)irep->lv); // discard const
    irep->lv = new_lv;
  }

  // Debug info filenames
  if (irep->debug_info) {
    for (int i = 0; i < irep->debug_info->flen; i++) {
      mrc_irep_debug_info_file *file = irep->debug_info->files[i];
      pm_constant_t *constant = pm_constant_pool_id_to_constant(constant_pool, file->filename_sym);
      file->filename_sym = (mrc_sym)intern_kept(cc, keep, constant);
    }
  }

  for (int i = 0; i < irep->rlen; i++) {
    resolve_intern(cc, (mrc_irep *)irep->reps[i], keep);
  }
}

MRC_API void
mrc_resolve_intern(mrc_ccontext *cc, mrc_irep *irep)
{
  if (cc->p->constant_pool.constants == NULL) {
    return;
  }
  int ai = mrb_gc_arena_save(cc->mrb);
  resolve_intern(cc, irep, mrb_ary_new(cc->mrb));
  mrb_gc_arena_restore(cc->mrb, ai);
}

MRC_API mrb_value
mrc_create_task(mrc_ccontext *cc, mrc_irep *irep, mrb_value name, mrb_value priority, mrb_value top_self)
{
  mrc_resolve_intern(cc, irep);
  struct RProc *proc = mrb_proc_new(cc->mrb, (mrb_irep *)irep);
  proc->c = NULL;
  return mrb_create_task(cc->mrb, proc, name, priority, top_self);
}


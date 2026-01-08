#include <mruby.h>
#include <mruby/string.h>
#include <mruby/presym.h>
#include <mruby/internal.h>
#include <mruby/class.h>

#include "mrc_utils.h"

// From mruby-binding gem
extern const struct RProc *mrb_binding_extract_proc(mrb_state *mrb, mrb_value binding);
extern struct REnv *mrb_binding_extract_env(mrb_state *mrb, mrb_value binding);

static struct REnv*
mrb_env_new(mrb_state *mrb, struct mrb_context *c, mrb_callinfo *ci, int nstacks, mrb_value *stack, struct RClass *tc)
{
  struct REnv *e;
  mrb_int bidx = 1;
  int n = ci->n;
  int nk = ci->nk;

  e = MRB_OBJ_ALLOC(mrb, MRB_TT_ENV, NULL);
  e->c = tc;
  MRB_ENV_SET_LEN(e, nstacks);
  bidx += (n == 15) ? 1 : n;
  bidx += (nk == 15) ? 1 : (2*nk);
  MRB_ENV_SET_BIDX(e, bidx);
  e->mid = ci->mid;
  e->stack = stack;
  e->cxt = c;
  MRB_ENV_COPY_FLAGS_FROM_CI(e, ci);

  return e;
}

static struct RProc*
create_proc_from_string(mrb_state *mrb, const char *s, mrb_int len, mrb_value binding, const char *file, mrb_int line)
{
  mrc_ccontext *cc;
  struct RProc *proc;
  const struct RProc *scope;
  struct REnv *e;
  mrb_callinfo *ci; /* callinfo of eval caller */
  struct RClass *target_class = NULL;
  struct mrb_context *c = mrb->c;

  if (!mrb_nil_p(binding)) {
    scope = mrb_binding_extract_proc(mrb, binding);
    if (MRB_PROC_CFUNC_P(scope)) {
      e = NULL;
    }
    else {
      e = mrb_binding_extract_env(mrb, binding);
    }
  }
  else {
    ci = (c->ci > c->cibase) ? c->ci - 1 : c->cibase;
    scope = ci->proc;
    e = NULL;
  }

  if (file) {
    if (strlen(file) >= UINT16_MAX) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "filename too long");
    }
  }
  else {
    file = "(eval)";
  }

  cc = mrc_ccontext_new(mrb);
  cc->lineno = (uint16_t)line;
  mrc_ccontext_filename(cc, file);
  cc->capture_errors = FALSE;//TRUE; TODO!
  cc->no_optimize = TRUE;
  cc->upper = scope && MRB_PROC_CFUNC_P(scope) ? NULL : scope;

  const uint8_t *code = (const uint8_t *)s;
  mrc_irep *irep = mrc_load_string_cxt(cc, &code, len);

  if (!irep) {
    mrc_ccontext_free(cc);
    mrb_raisef(mrb, E_SYNTAX_ERROR, "eval string syntax error: %s", s);
  }

  mrc_resolve_intern(cc, irep);
  proc = mrb_proc_new(mrb, irep);

  if (proc == NULL) {
    /* codegen error */
    mrc_ccontext_free(cc);
    mrb_raise(mrb, E_SCRIPT_ERROR, "codegen error");
  }
  if (c->ci > c->cibase) {
    ci = &c->ci[-1];
  }
  else {
    ci = c->cibase;
  }
  if (scope) {
    target_class = MRB_PROC_TARGET_CLASS(scope);
    if (!MRB_PROC_CFUNC_P(scope)) {
      if (e == NULL) {
        /* when `binding` is nil */
        e = mrb_vm_ci_env(ci);
        if (e == NULL) {
          e = mrb_env_new(mrb, c, ci, ci->proc->body.irep->nlocals, ci->stack, target_class);
          ci->u.env = e;
        }
      }
      proc->e.env = e;
      proc->flags |= MRB_PROC_ENVSET;
      mrb_field_write_barrier(mrb, (struct RBasic*)proc, (struct RBasic*)e);
    }
  }
  proc->upper = scope;
  mrb_vm_ci_target_class_set(mrb->c->ci, target_class);
  /* mrb_codedump_all(mrb, proc); */

//  mrc_irep_free(cc, irep);
  mrc_ccontext_free(cc);

  return proc;
}

static mrb_value
exec_irep(mrb_state *mrb, mrb_value self, struct RProc *proc)
{
  mrb_callinfo *ci = mrb->c->ci;

  /* no argument passed from eval() */
  ci->n = 0;
  ci->nk = 0;
  /* clear visibility */
  MRB_CI_SET_VISIBILITY_BREAK(ci);
  /* clear block */
  ci->stack[1] = mrb_nil_value();
  return mrb_exec_irep(mrb, self, proc);
}


static mrb_value
mrb_kernel_eval(mrb_state *mrb, mrb_value self)
{
  const char *script;
  mrb_int len;
  mrb_get_args(mrb, "s", &script, &len);
  struct RProc *proc = create_proc_from_string(mrb, script, len, mrb_nil_value(), NULL, 0);
  mrb_assert(!MRB_PROC_CFUNC_P(proc));
  return exec_irep(mrb, self, proc);
}

static mrb_value
mrb_binding_eval(mrb_state *mrb, mrb_value self)
{
  const char *script;
  mrb_int len;
  mrb_get_args(mrb, "s", &script, &len);

  // Get receiver from binding
  mrb_value recv = mrb_iv_get(mrb, self, MRB_SYM(recv));

  struct RProc *proc = create_proc_from_string(mrb, script, len, self, NULL, 0);
  mrb_assert(!MRB_PROC_CFUNC_P(proc));
  return exec_irep(mrb, recv, proc);
}

void
mrb_picoruby_eval_gem_init(mrb_state *mrb)
{
  mrb_define_private_method_id(mrb, mrb->kernel_module, MRB_SYM(eval), mrb_kernel_eval, MRB_ARGS_REQ(1));

  // Add Binding#eval if Binding class exists (requires mruby-binding)
  struct RClass *binding_class = mrb_class_get_id(mrb, MRB_SYM(Binding));
  if (binding_class) {
    mrb_define_method_id(mrb, binding_class, MRB_SYM(eval), mrb_binding_eval, MRB_ARGS_REQ(1));
  }
}

void
mrb_picoruby_eval_gem_final(mrb_state* mrb)
{
}

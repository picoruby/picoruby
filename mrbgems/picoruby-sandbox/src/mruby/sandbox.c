#include <errno.h>
#include <stdlib.h>
#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/string.h>
#include <mruby/variable.h>
// #include <mruby/debug.h>

// Workaround for picoruby.h defines MRUBY_IREP_H
void mrb_irep_incref(mrb_state *, struct mrb_irep *);
void mrb_irep_decref(mrb_state *, struct mrb_irep *);
void mrb_irep_cutref(mrb_state *, struct mrb_irep *);

static void
mrb_sandbox_state_free(mrb_state *mrb, void *ptr) {
  SandboxState *ss = (SandboxState *)ptr;
  mrb_gc_unregister(mrb, ss->task);
  mrb_free(mrb, ss);
}
struct mrb_data_type mrb_sandbox_state_type = {
  "SandboxState", mrb_sandbox_state_free
};

#define SS() \
  SandboxState *ss = (SandboxState *)mrb_data_get_ptr(mrb, self, &mrb_sandbox_state_type)

static mrb_value
mrb_sandbox_initialize(mrb_state *mrb, mrb_value self)
{
  SandboxState *ss = (SandboxState *)mrb_malloc(mrb, sizeof(SandboxState));
  memset(ss, 0, sizeof(SandboxState));
  DATA_PTR(self) = ss;
  DATA_TYPE(self) = &mrb_sandbox_state_type;

  ss->cc = mrc_ccontext_new(mrb);

  const uint8_t *script = (const uint8_t *)"Task.current.suspend";
  size_t size = strlen((const char *)script);
  ss->irep = mrc_load_string_cxt(ss->cc, (const uint8_t **)&script, size);

  mrb_value name;
  mrb_get_args(mrb, "|S", &name);
  if (mrb_nil_p(name)) {
    name = mrb_str_new_cstr(mrb, "sandbox");
  }
  mrb_iv_set(mrb, self, MRB_IVSYM(name), name);

  mrb_value task = mrc_create_task(ss->cc, ss->irep, name, mrb_nil_value(), mrb_obj_value(mrb->object_class));

  mrb_gc_register(mrb, task);
  ss->task = task;

  return self;
}

static mrb_bool
sandbox_compile_sub(mrb_state *mrb, SandboxState *ss, const uint8_t *script, const size_t size, mrb_value remove_lv)
{
  free_ccontext(ss);
  init_options(ss->options);
  ss->cc = mrc_ccontext_new(mrb);
  ss->cc->options = ss->options;
  // TODO: Ask Matz
  // if (ss->irep) mrb_irep_decref(mrb, ss->irep);
  ss->irep = mrc_load_string_cxt(ss->cc, (const uint8_t **)&script, size);
  if (ss->irep && mrb_test(remove_lv)) mrc_irep_remove_lv(ss->cc, ss->irep);
  mrb_irep_incref(mrb, ss->irep);
  ss->options = ss->cc->options;
  ss->cc->options = NULL;
  if (!ss->irep) {
    free_ccontext(ss);
    return FALSE;
  }
//  mrb_debug_info_alloc(mrb, ss->irep);
  return TRUE;
}

static mrb_value
mrb_sandbox_compile(mrb_state *mrb, mrb_value self)
{
  SS();
  const char *script;

  uint32_t kw_num = 1;
  uint32_t kw_required = 0;
  mrb_sym kw_names[] = { MRB_SYM(remove_lv) };
  mrb_value kw_values[kw_num];
  mrb_kwargs kwargs = { kw_num, kw_required, kw_names, kw_values, NULL };

  mrb_get_args(mrb, "z:", &script, &kwargs);
  if (mrb_undef_p(kw_values[0])) { kw_values[0] = mrb_false_value(); }

  const size_t size = strlen(script);
  if (!sandbox_compile_sub(mrb, ss, (const uint8_t *)script, size, kw_values[0])) {
    return mrb_false_value();
  }
  return mrb_true_value();
}

static mrb_value
mrb_sandbox_compile_from_memory(mrb_state *mrb, mrb_value self)
{
  SS();
  mrb_int address;
  mrb_int size;

  uint32_t kw_num = 1;
  uint32_t kw_required = 0;
  mrb_sym kw_names[] = { MRB_SYM(remove_lv) };
  mrb_value kw_values[kw_num];
  mrb_kwargs kwargs = { kw_num, kw_required, kw_names, kw_values, NULL };

  mrb_get_args(mrb, "ii:", &address, &size, &kwargs);
  if (size <= 0) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid size: %S", mrb_fixnum_value(size));
  }
  if (address <= 0) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid address: %S", mrb_fixnum_value(address));
  }
  if (mrb_undef_p(kw_values[0])) { kw_values[0] = mrb_false_value(); }

  if (!sandbox_compile_sub(mrb, ss, (const uint8_t *)(uintptr_t)address, size, kw_values[0])) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to compile script");
  }
  return mrb_true_value();
}

static mrb_value
mrb_sandbox_resume(mrb_state *mrb, mrb_value self)
{
  SS();
  mrb_resume_task(mrb, ss->task);
  return mrb_true_value();
}

static mrb_value
mrb_sandbox_execute(mrb_state *mrb, mrb_value self)
{
  SS();
  mrc_resolve_intern(ss->cc, ss->irep);
  struct RProc *proc = mrb_proc_new(mrb, ss->irep);
  proc->e.target_class = mrb->object_class;
  mrb_task_proc_set(mrb, ss->task, proc);
  mrb_task_reset_context(mrb, ss->task);

  mrb_resume_task(mrb, ss->task);
  return mrb_true_value();
}

static mrb_value
mrb_sandbox_state(mrb_state *mrb, mrb_value self)
{
  SS();
  return mrb_task_status(mrb, ss->task);
}

static mrb_value
mrb_sandbox_result(mrb_state *mrb, mrb_value self)
{
  SS();

  pm_options_t *options = (pm_options_t *)mrc_calloc(ss->cc, 1, sizeof(pm_options_t));
  pm_options_scopes_init(options, 1);
  pm_options_scope_t *scope = &options->scopes[0];
  const struct mrc_irep *ir = ss->irep;
  size_t nlocals = ir->nlocals - 1; // exclude self
  pm_options_scope_init(scope, nlocals);
  const mrc_sym *v = ir->lv;
  if (v) {
    const char *name;
    for (size_t j = 0; j < nlocals; j++, v++) {
      name = mrb_sym_name(ss->cc->mrb, *v);
      pm_string_constant_init(&scope->locals[j], name, strlen(name));
      if (name == ss->cc->mrb->symbuf) {
        pm_string_ensure_owned(&scope->locals[j]); // copy name
      }
    }
  }
  if (ss->options) {
    pm_options_free(ss->options);
    mrc_free(ss->cc, ss->options);
  }
  ss->options = options;

  return mrb_task_value(mrb, ss->task);
}

static mrb_value
mrb_sandbox_error(mrb_state *mrb, mrb_value self)
{
  SS();
  mrb_value value = mrb_task_value(mrb, ss->task);
  if (mrb_obj_is_kind_of(mrb, value, mrb->eException_class)) {
    return value;
  }
  else {
    return mrb_nil_value();
  }
}

static mrb_value
mrb_sandbox_stop(mrb_state *mrb, mrb_value self)
{
  SS();
  if (!mrb_stop_task(mrb, ss->task)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Already stopped");
  }
  return mrb_true_value();
}

static mrb_value
mrb_sandbox_suspend(mrb_state *mrb, mrb_value self)
{
  SS();
  mrb_suspend_task(mrb, ss->task);
  return mrb_nil_value();
}

static mrb_value
mrb_sandbox_free_parser(mrb_state *mrb, mrb_value self)
{
  mrb_notimplement(mrb);
  return mrb_nil_value();
}

static mrb_bool
sandbox_exec_vm_code_sub(mrb_state *mrb, SandboxState *ss)
{
  struct RProc *proc = mrb_proc_new(mrb, ss->irep);
  proc->e.target_class = mrb->object_class;
  proc->c = NULL;
  mrb_task_init_context(mrb, ss->task, proc);
  mrb_resume_task(mrb, ss->task);
  return TRUE;
}

// created in mruby/src/load.c
mrb_irep *mrb_read_irep(mrb_state *mrb, const uint8_t *bin);

static mrb_value
mrb_sandbox_exec_vm_code(mrb_state *mrb, mrb_value self)
{
  SS();
  mrb_value vm_code;
  mrb_get_args(mrb, "S", &vm_code);
  const uint8_t *code = (const uint8_t *)RSTRING_PTR(vm_code);
  if (ss->irep) mrc_irep_free(ss->cc, ss->irep);
  ss->irep = mrb_read_irep(mrb, code);
  if (sandbox_exec_vm_code_sub(mrb, ss)) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_sandbox_exec_vm_code_from_memory(mrb_state *mrb, mrb_value self)
{
  SS();
  mrb_int address;
  mrb_get_args(mrb, "i", &address);
  if (ss->irep) mrc_irep_free(ss->cc, ss->irep);
  ss->irep = mrb_read_irep(mrb, (const uint8_t *)(uintptr_t)address);
  if (sandbox_exec_vm_code_sub(mrb, ss)) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_sandbox_terminate(mrb_state *mrb, mrb_value self)
{
  SS();
  mrb_terminate_task(mrb, ss->task);
  return mrb_nil_value();
}


void
mrb_picoruby_sandbox_gem_init(mrb_state *mrb)
{
  struct RClass *class_Sandbox = mrb_define_class_id(mrb, MRB_SYM(Sandbox), mrb->object_class);

  MRB_SET_INSTANCE_TT(class_Sandbox, MRB_TT_CDATA);

  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(initialize), mrb_sandbox_initialize, MRB_ARGS_OPT(1));
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(compile), mrb_sandbox_compile, MRB_ARGS_REQ(1)|MRB_ARGS_KEY(1,1));
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(compile_from_memory), mrb_sandbox_compile_from_memory, MRB_ARGS_REQ(2)|MRB_ARGS_KEY(1,1));
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(resume), mrb_sandbox_resume, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(execute), mrb_sandbox_execute, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(state), mrb_sandbox_state, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(result), mrb_sandbox_result, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(error), mrb_sandbox_error, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(stop), mrb_sandbox_stop, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(suspend), mrb_sandbox_suspend, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(free_parser), mrb_sandbox_free_parser, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(exec_mrb), mrb_sandbox_exec_vm_code, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(exec_mrb_from_memory), mrb_sandbox_exec_vm_code_from_memory, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(terminate), mrb_sandbox_terminate, MRB_ARGS_NONE());
}

void
mrb_picoruby_sandbox_gem_final(mrb_state* mrb)
{
}

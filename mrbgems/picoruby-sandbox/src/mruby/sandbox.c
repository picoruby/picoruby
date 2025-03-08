#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/string.h>

static void
mrb_sandbox_state_free(mrb_state *mrb, void *ptr) {
  mrb_free(mrb, ptr);
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
//  free_ccontext(ss);

  ss->tcb = mrc_create_task(ss->cc, ss->irep, NULL);
  ss->tcb->flag_permanence = 1;
  {
    char *name;
    mrb_get_args(mrb, "|z", &name);
    // TODO name should be ivar of Task instance
    //mrbc_set_task_name(ss->tcb, name == NULL ? "sandbox" : name);
  }
  return self;
}

static mrb_bool
sandbox_compile_sub(mrb_state *mrb, SandboxState *ss, const uint8_t *script, const size_t size, mrb_value remove_lv)
{
  free_ccontext(ss);
  init_options(ss->options);
  ss->cc = mrc_ccontext_new(mrb);
  ss->cc->options = ss->options;
  ss->irep = mrc_load_string_cxt(ss->cc, (const uint8_t **)&script, size);
  if (ss->irep && mrb_test(remove_lv)) mrc_irep_remove_lv(ss->cc, ss->irep);
  ss->options = ss->cc->options;
  ss->cc->options = NULL;
  if (!ss->irep) {
    free_ccontext(ss);
    return FALSE;
  }
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

  mrb_get_args(mrb, "z", &script, &kwargs);
  if (mrb_undef_p(kw_values[0])) { kw_values[0] = mrb_true_value(); }

  const size_t size = strlen(script);
  if (!sandbox_compile_sub(mrb, ss, (const uint8_t *)script, size, kw_values[0])) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to compile script");
  }
  return mrb_true_value();
}

static mrb_value
mrb_sandbox_compile_from_memory(mrb_state *mrb, mrb_value self)
{
  SS();
  const mrb_int *address;
  const size_t size;

  uint32_t kw_num = 1;
  uint32_t kw_required = 0;
  mrb_sym kw_names[] = { MRB_SYM(remove_lv) };
  mrb_value kw_values[kw_num];
  mrb_kwargs kwargs = { kw_num, kw_required, kw_names, kw_values, NULL };

  mrb_get_args(mrb, "ii:", &address, &size, &kwargs);
  if (mrb_undef_p(kw_values[0])) { kw_values[0] = mrb_true_value(); }

  if (!sandbox_compile_sub(mrb, ss, (const uint8_t *)(intptr_t)address, size, kw_values[0])) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to compile script");
  }
  return mrb_true_value();
}

static void
reset_context(struct mrb_context *c)
{
  c->ci = c->cibase;
  c->status = MRB_TASK_CREATED;
}

static mrb_value
mrb_sandbox_execute(mrb_state *mrb, mrb_value self)
{
  SS();
  mrc_resolve_intern(ss->cc, ss->irep);
  struct RProc *proc = mrb_proc_new(mrb, ss->irep);
  proc->c = NULL;
  mrb_tcb_init_context(mrb, &ss->tcb->c, proc);
  reset_context(&ss->tcb->c); mrb_resume_task(mrb, ss->tcb);
  return mrb_true_value();
}

static mrb_value
mrb_sandbox_state(mrb_state *mrb, mrb_value self)
{
  SS();
  return mrb_int_value(mrb, ss->tcb->state);
}

static mrb_value
mrb_sandbox_result(mrb_state *mrb, mrb_value self)
{
  SS();
  mrb_value last_value = *(ss->tcb->c.stend - 1);
  return last_value;
}

static mrb_value
mrb_sandbox_error(mrb_state *mrb, mrb_value self)
{
  if (mrb->exc) {
    return mrb_obj_value(mrb->exc);
  }
  else {
    return mrb_nil_value();
  }
}

static mrb_value
mrb_sandbox_interrupt(mrb_state *mrb, mrb_value self)
{
  mrb_notimplement(mrb);
  return mrb_nil_value();
}

static mrb_value
mrb_sandbox_suspend(mrb_state *mrb, mrb_value self)
{
  SS();
  mrb_suspend_task(mrb, ss->tcb);
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
  proc->c = NULL;
  mrb_tcb_init_context(mrb, &ss->tcb->c, proc);
  reset_context(&ss->tcb->c);
  mrb_resume_task(mrb, ss->tcb);
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
  const mrb_int *address;
  mrb_get_args(mrb, "i", &address);
  ss->irep = mrb_read_irep(mrb, (const uint8_t *)(intptr_t)address);
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
  mrb_terminate_task(mrb, ss->tcb);
  return mrb_nil_value();
}


void
mrb_picoruby_sandbox_gem_init(mrb_state *mrb)
{
  struct RClass *class_Sandbox = mrb_define_class_id(mrb, MRB_SYM(Sandbox), mrb->object_class);

  MRB_SET_INSTANCE_TT(class_Sandbox, MRB_TT_CDATA);

  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(initialize), mrb_sandbox_initialize, MRB_ARGS_OPT(1));
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(compile), mrb_sandbox_compile, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(compile_from_memory), mrb_sandbox_compile_from_memory, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(execute), mrb_sandbox_execute, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(state), mrb_sandbox_state, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(result), mrb_sandbox_result, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(error), mrb_sandbox_error, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(interrupt), mrb_sandbox_interrupt, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(suspend), mrb_sandbox_suspend, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(free_parser), mrb_sandbox_free_parser, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(exec_mrb), mrb_sandbox_exec_vm_code, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(exec_mrb_from_memory), mrb_sandbox_exec_vm_code_from_memory, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_Sandbox, MRB_SYM(terminate), mrb_sandbox_terminate, MRB_ARGS_NONE());
}

void
mrb_picoruby_sandbox_gem_final(mrb_state* mrb)
{
}

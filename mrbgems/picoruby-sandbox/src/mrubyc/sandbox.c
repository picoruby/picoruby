#include <mrubyc.h>

#define SS() \
  SandboxState *ss = (SandboxState *)v->instance->data

static void
c_sandbox_state(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SS();
  SET_INT_RETURN(ss->tcb->state);
}

static void
c_sandbox_error(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SS();
  mrbc_vm *sandbox_vm = (mrbc_vm *)&ss->tcb->vm;
  if (sandbox_vm->exception.tt == MRBC_TT_NIL) {
    SET_NIL_RETURN();
  } else {
    SET_RETURN(sandbox_vm->exception);
  }
}

static void
c_sandbox_result(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SS();
  mrbc_vm *sandbox_vm = (mrbc_vm *)&ss->tcb->vm;
  if (sandbox_vm->regs[ss->cc->scope_sp].tt == MRBC_TT_EMPTY) {
    /*
     * This bug was fixed in
     * https://github.com/picoruby/mruby-pico-compiler/commit/4f39ddc
     * but I leave this workaround in case of the bug is still there.
     */
    console_printf("Oops, return value is gone\n");
    SET_NIL_RETURN();
  } else {
    SET_RETURN(sandbox_vm->regs[ss->cc->scope_sp]);
  }
}

static void
c_sandbox_interrupt(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value *abort = mrbc_get_class_const(v->instance->cls, mrbc_search_symid("Abort"));
  SS();
  mrbc_vm *sandbox_vm = (mrbc_vm *)&ss->tcb->vm;
  mrbc_raise(sandbox_vm, abort->cls, "interrupt");
}

static void
c_sandbox_free_parser(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SS();
  { /*
       Workaround but causes memory leak 😔
       To preserve symbol table
    */
    if (ss->vm_code) ss->vm_code = NULL;
  }
  free_ccontext(ss);
  SET_NIL_RETURN();
}

static void
c_sandbox_suspend(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SS();
  mrbc_suspend_task(ss->tcb);
  SET_NIL_RETURN();
}

static void
sandbox_compile_sub(mrbc_vm *vm, mrbc_value *v, uint8_t *script, size_t size)
{
  SS();
  free_ccontext(ss);
  init_options(ss->options);
  ss->cc = mrc_ccontext_new(NULL);
  ss->cc->options = ss->options;
  ss->irep = mrc_load_string_cxt(ss->cc, (const uint8_t **)&script, size);
  if (ss->irep) mrc_irep_remove_lv(ss->cc, ss->irep);
  ss->options = ss->cc->options;
  ss->cc->options = NULL;
  if (!ss->irep) {
    free_ccontext(ss);
    SET_FALSE_RETURN();
  }
  else {
    size_t vm_code_size = 0;
    int result = mrc_dump_irep(ss->cc, (const mrc_irep *)ss->irep, 0, &ss->vm_code, &vm_code_size);
    if (result != MRC_DUMP_OK) {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Dump failed");
      free_ccontext(ss);
      return;
    }
    SET_TRUE_RETURN();
  }
}

static void
c_sandbox_compile(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint8_t *script = GET_STRING_ARG(1);
  size_t size = strlen((const char *)script);
  sandbox_compile_sub(vm, v, script, size);
}

static void
c_sandbox_compile_from_memory(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint8_t *script = (uint8_t *)(intptr_t)GET_INT_ARG(1);
  size_t size = GET_INT_ARG(2);
  sandbox_compile_sub(vm, v, script, size);
}

static void
reset_vm(mrbc_vm *vm)
{
  vm->cur_irep        = vm->top_irep;
  vm->inst            = vm->cur_irep->inst;
  vm->cur_regs        = vm->regs;
  vm->target_class    = mrbc_class_object;
  vm->callinfo_tail   = NULL;
  vm->ret_blk         = NULL;
  vm->exception       = mrbc_nil_value();
  vm->flag_preemption = 0;
  vm->flag_stop       = 0;
}

static bool
sandbox_exec_mrb_sub(mrbc_vm *sandbox_vm, SandboxState *ss)
{
  if (mrbc_load_mrb(sandbox_vm, ss->vm_code) != 0) {
    return false;
  } else {
    reset_vm(sandbox_vm);
    mrbc_resume_task(ss->tcb);
    return true;
  }
}

static void
c_sandbox_exec_mrb(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SS();
  mrbc_vm *sandbox_vm = (mrbc_vm *)&ss->tcb->vm;
  mrbc_value mrb = v[1];
  ss->vm_code = mrb.string->data;
  mrb.string->data = NULL;
  mrb.string->size = 0;
  if (sandbox_exec_mrb_sub(sandbox_vm, ss)) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_sandbox_exec_mrb_from_memory(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SS();
  mrbc_vm *sandbox_vm = (mrbc_vm *)&ss->tcb->vm;
  ss->vm_code = (uint8_t *)(intptr_t)GET_INT_ARG(1);
  if (sandbox_exec_mrb_sub(sandbox_vm, ss)) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_sandbox_execute(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SS();
  mrbc_vm *sandbox_vm = (mrbc_vm *)&ss->tcb->vm;
  if(mrbc_load_mrb(sandbox_vm, ss->vm_code) != 0) {
    SET_FALSE_RETURN();
  } else {
    reset_vm(sandbox_vm);
    mrbc_resume_task(ss->tcb);
    SET_TRUE_RETURN();
  }
}

static void
c_sandbox_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  static uint8_t *sandbox_task = NULL;
  mrbc_value sandbox = mrbc_instance_new(vm, v->cls, sizeof(SandboxState));
  SandboxState *ss = (SandboxState *)sandbox.instance->data;
  memset(ss, 0, sizeof(SandboxState));

  if (!sandbox_task) {
    ss->cc = mrc_ccontext_new(NULL);

    const uint8_t *script = (const uint8_t *)"Task.current.suspend";
    size_t size = strlen((const char *)script);
    ss->irep = mrc_load_string_cxt(ss->cc, (const uint8_t **)&script, size);
    uint8_t flags = 0;
    size_t vm_code_size = 0;
    int result = mrc_dump_irep(ss->cc, (const mrc_irep *)ss->irep, flags, &ss->vm_code, &vm_code_size);
    if (result != MRC_DUMP_OK) {
      free_ccontext(ss);
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Dump failed");
      return;
    }
    sandbox_task = ss->vm_code;
    mrc_irep_free(ss->cc, ss->irep);
    free_ccontext(ss);
  }
  if (!sandbox_task) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to compile sandbox_task");
    return;
  }

  ss->tcb = mrbc_tcb_new(MAX_REGS_SIZE, MRBC_TASK_DEFAULT_STATE, MRBC_TASK_DEFAULT_PRIORITY);
  mrbc_create_task(sandbox_task, ss->tcb);
  ss->tcb->vm.flag_permanence = 1;
  const char *name;
  if (0 == argc) {
    name = "sandbox";
  } else {
    name = (const char *)GET_STRING_ARG(1);
  }
  mrbc_set_task_name(ss->tcb, name);
  SET_RETURN(sandbox);
}

static void
c_sandbox_terminate(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SS();
  mrbc_terminate_task(ss->tcb);
  SET_NIL_RETURN();
}

void
mrbc_sandbox_init(mrbc_vm *vm)
{
  mrbc_class *mrbc_class_Sandbox = mrbc_define_class(vm, "Sandbox", mrbc_class_object);
  mrbc_define_method(vm, mrbc_class_Sandbox, "compile", c_sandbox_compile);
  mrbc_define_method(vm, mrbc_class_Sandbox, "compile_from_memory", c_sandbox_compile_from_memory);
  mrbc_define_method(vm, mrbc_class_Sandbox, "execute", c_sandbox_execute);
  mrbc_define_method(vm, mrbc_class_Sandbox, "state",   c_sandbox_state);
  mrbc_define_method(vm, mrbc_class_Sandbox, "result",  c_sandbox_result);
  mrbc_define_method(vm, mrbc_class_Sandbox, "error",   c_sandbox_error);
  mrbc_define_method(vm, mrbc_class_Sandbox, "interrupt", c_sandbox_interrupt);
  mrbc_define_method(vm, mrbc_class_Sandbox, "suspend", c_sandbox_suspend);
  mrbc_define_method(vm, mrbc_class_Sandbox, "free_parser", c_sandbox_free_parser);
  mrbc_define_method(vm, mrbc_class_Sandbox, "exec_mrb", c_sandbox_exec_mrb);
  mrbc_define_method(vm, mrbc_class_Sandbox, "exec_mrb_from_memory", c_sandbox_exec_mrb_from_memory);
  mrbc_define_method(vm, mrbc_class_Sandbox, "new",     c_sandbox_new);
  mrbc_define_method(vm, mrbc_class_Sandbox, "terminate", c_sandbox_terminate);
}

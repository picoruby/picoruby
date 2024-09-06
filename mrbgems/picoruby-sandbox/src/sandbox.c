#include <mruby_compiler.h>
#include <mrubyc.h>
#include "sandbox.h"

#ifndef NODE_BOX_SIZE
#define NODE_BOX_SIZE 20
#endif

typedef struct sandbox_state {
  mrbc_tcb *tcb;
  mrc_ccontext *c;
  mrc_irep *irep;
  uint8_t *vm_code;
} SandboxState;

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
  if (sandbox_vm->regs[ss->c->scope_sp].tt == MRBC_TT_EMPTY) {
    /*
     * This bug was fixed in
     * https://github.com/picoruby/mruby-pico-compiler/commit/4f39ddc
     * but I leave this workaround in case of the bug is still there.
     */
    console_printf("Oops, return value is gone\n");
    SET_NIL_RETURN();
  } else {
    SET_RETURN(sandbox_vm->regs[ss->c->scope_sp]);
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
  mrc_ccontext_free(ss->c);
}

static void
c_sandbox_suspend(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SS();
  mrbc_suspend_task(ss->tcb);
}

static void
c_sandbox_compile(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SS();
  //ss->c->p = (ParserState *)PICORBC_ALLOC(sizeof(ParserState));
  //Compiler_parseInitState(ss->c->p, NODE_BOX_SIZE);
  //StreamInterface *si = StreamInterface_new(NULL, (const char *)GET_STRING_ARG(1), STREAM_TYPE_MEMORY);
  //if (!Compiler_compile(ss->c->p, si, &ss->cxt)) {
  //  SET_FALSE_RETURN();
  //  Scope *upper_scope = ss->c->p->scope;
  //  while (upper_scope->upper) upper_scope = upper_scope->upper;
  //  upper_scope->lvar = NULL; /* top level lvar (== cxt->sysm) should not be freed */
  //  Compiler_parserStateFree(ss->c->p);
  //} else {
  //  SET_TRUE_RETURN();
  //}
  //StreamInterface_free(si);
  ss->c = mrc_ccontext_new(NULL);
  uint8_t *script = GET_STRING_ARG(1);
  size_t size = strlen((const char *)script);
  ss->irep = mrc_load_string_cxt(ss->c, (const uint8_t **)&script, size);
  if (!ss->irep) {
    mrc_ccontext_free(ss->c);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Compile failed");
    SET_FALSE_RETURN();
  }
  else {
    SET_TRUE_RETURN();
  }
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

static void
c_sandbox_exec_mrb(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SS();
  mrbc_vm *sandbox_vm = (mrbc_vm *)&ss->tcb->vm;
  mrbc_value mrbc_string = v[1];
  if (mrbc_load_mrb(sandbox_vm, mrbc_string.string->data) != 0) {
    SET_FALSE_RETURN();
  } else {
    reset_vm(sandbox_vm);
    mrbc_resume_task(ss->tcb);
    SET_TRUE_RETURN();
  }
}

static void
c_sandbox_execute(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SS();
  mrbc_vm *sandbox_vm = (mrbc_vm *)&ss->tcb->vm;
  if(mrbc_load_mrb(sandbox_vm, ss->vm_code) != 0) {
    mrc_ccontext_free(ss->c);
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
  /*
   * FIXME: Remove global variable `loglevel`
   */
//  if (argc == 1 && GET_TT_ARG(1) == MRBC_TT_TRUE) {
//    loglevel = LOGLEVEL_FATAL;
//  }
  mrbc_value sandbox = mrbc_instance_new(vm, v->cls, sizeof(SandboxState));
  SandboxState *ss = (SandboxState *)sandbox.instance->data;

  if (!sandbox_task) {
    //ParserState *p = Compiler_parseInitState(NULL, NODE_BOX_SIZE);
    //StreamInterface *si = StreamInterface_new(NULL, "Task.current.suspend", STREAM_TYPE_MEMORY);
    //Compiler_compile(p, si, NULL);
    //sandbox_task = p->scope->vm_code;
    //p->scope->vm_code = NULL;
    //Compiler_parserStateFree(p);
    //StreamInterface_free(si);
    ss->c = mrc_ccontext_new(NULL);
    const uint8_t *script = (const uint8_t *)"Task.current.suspend";
    size_t size = strlen((const char *)script);
    ss->irep = mrc_load_string_cxt(ss->c, (const uint8_t **)&script, size);
    uint8_t flags = 0;
    size_t vm_code_size = 0;
    int result = mrc_dump_irep(ss->c, (const mrc_irep *)ss->irep, flags, &ss->vm_code, &vm_code_size);
    if (result != MRC_DUMP_OK) {
      mrc_ccontext_free(ss->c);
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Dump failed");
      return;
    }
    sandbox_task = ss->vm_code;
    mrc_irep_free(ss->c, ss->irep);
  }
  if (!sandbox_task) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to compile sandbox_task");
    return;
  }

  ss->tcb = mrbc_tcb_new(MAX_REGS_SIZE, MRBC_TASK_DEFAULT_STATE, MRBC_TASK_DEFAULT_PRIORITY);
  mrbc_create_task(sandbox_task, ss->tcb);
  ss->tcb->vm.flag_permanence = 1;
  ss->c = mrc_ccontext_new(NULL);
  SET_RETURN(sandbox);
}

void
mrbc_sandbox_init(void)
{
  mrbc_class *mrbc_class_Sandbox = mrbc_define_class(0, "Sandbox", mrbc_class_object);
  mrbc_define_method(0, mrbc_class_Sandbox, "compile", c_sandbox_compile);
  mrbc_define_method(0, mrbc_class_Sandbox, "execute", c_sandbox_execute);
  mrbc_define_method(0, mrbc_class_Sandbox, "state",   c_sandbox_state);
  mrbc_define_method(0, mrbc_class_Sandbox, "result",  c_sandbox_result);
  mrbc_define_method(0, mrbc_class_Sandbox, "error",   c_sandbox_error);
  mrbc_define_method(0, mrbc_class_Sandbox, "interrupt", c_sandbox_interrupt);
  mrbc_define_method(0, mrbc_class_Sandbox, "suspend", c_sandbox_suspend);
  mrbc_define_method(0, mrbc_class_Sandbox, "free_parser", c_sandbox_free_parser);
  mrbc_define_method(0, mrbc_class_Sandbox, "exec_mrb", c_sandbox_exec_mrb);
  mrbc_define_method(0, mrbc_class_Sandbox, "new",     c_sandbox_new);
}

#include <picorbc.h>
#include <mrubyc.h>
#include "sandbox.h"

#ifndef NODE_BOX_SIZE
#define NODE_BOX_SIZE 20
#endif

typedef struct sandbox_state {
  ParserState *p;
  mrbc_tcb *tcb;
  picorbc_context cxt;
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
  if (sandbox_vm->regs[ss->p->scope->sp].tt == MRBC_TT_EMPTY) {
    /*
     * This bug was fixed in
     * https://github.com/picoruby/mruby-pico-compiler/commit/4f39ddc
     * but I leave this workaround in case of the bug is still there.
     */
    console_printf("Oops, return value is gone\n");
    SET_NIL_RETURN();
  } else {
    SET_RETURN(sandbox_vm->regs[ss->p->scope->sp]);
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
    if (ss->p->scope->vm_code) ss->p->scope->vm_code = NULL;
  }
  Compiler_parserStateFree(ss->p);
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
  ss->p = (ParserState *)PICORBC_ALLOC(sizeof(ParserState));
  Compiler_parseInitState(ss->p, NODE_BOX_SIZE);
  StreamInterface *si = StreamInterface_new(NULL, (const char *)GET_STRING_ARG(1), STREAM_TYPE_MEMORY);
  if (!Compiler_compile(ss->p, si, &ss->cxt)) {
    SET_FALSE_RETURN();
    Scope *upper_scope = ss->p->scope;
    while (upper_scope->upper) upper_scope = upper_scope->upper;
    upper_scope->lvar = NULL; /* top level lvar (== cxt->sysm) should not be freed */
    Compiler_parserStateFree(ss->p);
  } else {
    SET_TRUE_RETURN();
  }
  StreamInterface_free(si);
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
  if(mrbc_load_mrb(sandbox_vm, ss->p->scope->vm_code) != 0) {
    Compiler_parserStateFree(ss->p);
    SET_FALSE_RETURN();
  } else {
    reset_vm(sandbox_vm);
    mrbc_resume_task(ss->tcb);
    SET_TRUE_RETURN();
  }
}

/*
 * echo "suspend_task" > sandbox_task.rb
 * picorbc -Bsandbox_task -o- sandbox_task.rb
 */
static const uint8_t sandbox_task[] = {
0x52,0x49,0x54,0x45,0x30,0x33,0x30,0x30,0x00,0x00,0x00,0x52,0x4d,0x41,0x54,0x5a,
0x30,0x30,0x30,0x30,0x49,0x52,0x45,0x50,0x00,0x00,0x00,0x36,0x30,0x33,0x30,0x30,
0x00,0x00,0x00,0x2a,0x00,0x01,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,
0x2d,0x01,0x00,0x00,0x38,0x01,0x69,0x00,0x00,0x00,0x01,0x00,0x0c,0x73,0x75,0x73,
0x70,0x65,0x6e,0x64,0x5f,0x74,0x61,0x73,0x6b,0x00,0x45,0x4e,0x44,0x00,0x00,0x00,
0x00,0x08,
};

static void
c_sandbox_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value sandbox = mrbc_instance_new(vm, v->cls, sizeof(SandboxState));
  SandboxState *ss = (SandboxState *)sandbox.instance->data;
  ss->tcb = mrbc_tcb_new(MAX_REGS_SIZE, MRBC_TASK_DEFAULT_STATE, MRBC_TASK_DEFAULT_PRIORITY);
  mrbc_create_task(sandbox_task, ss->tcb);
  ss->tcb->vm.flag_permanence = 0;
  ss->tcb->vm.flag_protect_symbol_literal = 1;
  picorbc_context_new(&ss->cxt);
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

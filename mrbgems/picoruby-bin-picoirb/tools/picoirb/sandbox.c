#include <picorbc.h>
#include <mrubyc.h>
#include "sandbox.h"

#include "sandbox_task.c"

#ifndef NODE_BOX_SIZE
#define NODE_BOX_SIZE 20
#endif

mrbc_tcb *tcb_sandbox;

static ParserState *p;
static picorbc_context *cxt;

void
c_sandbox_state(mrb_vm *vm, mrb_value *v, int argc)
{
  SET_INT_RETURN(tcb_sandbox->state);
}

void
c_sandbox_result(mrb_vm *vm, mrb_value *v, int argc)
{
  mrbc_vm *sandbox_vm = (mrbc_vm *)&tcb_sandbox->vm;
  if (sandbox_vm->exception.tt == MRBC_TT_NIL) {
    SET_RETURN(sandbox_vm->regs[p->scope->sp]);
  } else {
    char message[] = "Error: Runtime error. code: __";
    snprintf(message, sizeof(message), "Error: Runtime error");
    SET_RETURN( mrbc_string_new_cstr(vm, message) );
  }
  mrbc_suspend_task(tcb_sandbox);
  { /*
       Workaround but causes memory leak 😔
       To preserve symbol table
    */
    p->scope->vm_code = NULL;
  }
  Compiler_parserStateFree(p);
}

void
c_sandbox_compile(mrb_vm *vm, mrb_value *v, int argc)
{
  p = Compiler_parseInitState(NODE_BOX_SIZE);
  //p->verbose = true;
  char script[255];
  sprintf(script, "_ = (%s)", (const char *)GET_STRING_ARG(1));
  StreamInterface *si = StreamInterface_new(NULL, script, STREAM_TYPE_MEMORY);
  if (!Compiler_compile(p, si, cxt)) {
    SET_FALSE_RETURN();
    p->scope->lvar = NULL; /* lvar (== cxt->sysm) should not be freed */
    Compiler_parserStateFree(p);
  } else {
    SET_TRUE_RETURN();
  }
  StreamInterface_free(si);
}

void
c_sandbox_resume(mrb_vm *vm, mrb_value *v, int argc)
{
  mrbc_vm *sandbox_vm = (mrbc_vm *)&tcb_sandbox->vm;
  if(mrbc_load_mrb(sandbox_vm, p->scope->vm_code) != 0) {
    Compiler_parserStateFree(p);
    SET_FALSE_RETURN();
  } else {
    sandbox_vm->cur_irep = sandbox_vm->top_irep;
    sandbox_vm->inst = sandbox_vm->cur_irep->inst;
    sandbox_vm->callinfo_tail = NULL;
    sandbox_vm->target_class = mrbc_class_object;
    sandbox_vm->flag_preemption = 0;
    mrbc_resume_task(tcb_sandbox);
    SET_TRUE_RETURN();
  }
}

void
create_sandbox(void)
{
  tcb_sandbox = mrbc_create_task(sandbox_task, 0);
  tcb_sandbox->vm.flag_permanence = 1;
  cxt = picorbc_context_new();
}

void
c_sandbox_exit(mrb_vm *vm, mrb_value *v, int argc)
{
  picorbc_context_free(cxt);
}


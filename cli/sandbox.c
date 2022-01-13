#include "../src/picorbc.h"
#include "../src/mrubyc/src/mrubyc.h"
#include "sandbox.h"

#include "ruby/sandbox.c"

#ifndef NODE_BOX_SIZE
#define NODE_BOX_SIZE 20
#endif

int loglevel = LOGLEVEL_ERROR;

mrbc_tcb *tcb_sandbox;

static ParserState *p;
static unsigned int nlocals;
static Symbol *symbol;
static Lvar *lvar;
static Literal *literal;
static unsigned int sp;
static unsigned int max_sp;
static StringPool *current_string_pool;

void
save_p_state(ParserState *p)
{
  nlocals = p->scope->nlocals;
  symbol  = p->scope->symbol;
  lvar    = p->scope->lvar;
  literal = p->scope->literal;
  sp      = p->scope->sp;
  max_sp  = p->scope->max_sp;
  current_string_pool    = p->current_string_pool;
  p->scope->symbol       = NULL;
  p->scope->lvar         = NULL;
  p->scope->literal      = NULL;
  p->current_string_pool = NULL;
}

void
restore_p_state(ParserState *p)
{
  p->scope->nlocals = nlocals;
  p->scope->symbol  = symbol;
  p->scope->lvar    = lvar;
  p->scope->literal = literal;
  p->scope->sp      = sp;
  p->scope->max_sp  = max_sp;
  p->current_string_pool = current_string_pool;
}

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
    SET_RETURN(sandbox_vm->regs[sp]);
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
c_sandbox_picorbc(mrb_vm *vm, mrb_value *v, int argc)
{
  p = Compiler_parseInitState(NODE_BOX_SIZE);
  if (tcb_sandbox) restore_p_state(p);
  StreamInterface *si = StreamInterface_new((char *)GET_STRING_ARG(1), STREAM_TYPE_MEMORY);
  if (!Compiler_compile(p, si)) {
    SET_FALSE_RETURN();
    save_p_state(p);
    Compiler_parserStateFree(p);
  } else {
    SET_TRUE_RETURN();
    save_p_state(p);
  }
  StreamInterface_free(si);
}

void
c_sandbox_resume(mrb_vm *vm, mrb_value *v, int argc)
{
  mrbc_vm *sandbox_vm = (mrbc_vm *)&tcb_sandbox->vm;
  if(mrbc_load_mrb(sandbox_vm, p->scope->vm_code) != 0) {
    SET_FALSE_RETURN();
  } else {
    sandbox_vm->cur_irep = sandbox_vm->top_irep;
    sandbox_vm->inst = sandbox_vm->cur_irep->inst;
//      sandbox_vm->current_regs = sandbox_vm->regs;
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
  p = Compiler_parseInitState(NODE_BOX_SIZE);
  save_p_state(p);
  Compiler_parserStateFree(p);
  tcb_sandbox = mrbc_create_task(sandbox, 0);
  tcb_sandbox->vm.flag_permanence = 1;
}


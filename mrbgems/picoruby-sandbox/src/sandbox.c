#include <picorbc.h>
#include <mrubyc.h>
#include "sandbox.h"

//#include "sandbox_task.c"

#ifndef NODE_BOX_SIZE
#define NODE_BOX_SIZE 20
#endif

typedef struct sandbox_state {
  ParserState *p;
  mrbc_tcb tcb;
  picorbc_context cxt;
} SandboxState;

#define SS() \
  SandboxState *ss = (SandboxState *)v->instance->data

static void
c_sandbox_state(mrb_vm *vm, mrb_value *v, int argc)
{
  SS();
  SET_INT_RETURN(ss->tcb.state);
}

static void
c_sandbox_error(mrb_vm *vm, mrb_value *v, int argc)
{
  SS();
  mrbc_vm *sandbox_vm = (mrbc_vm *)&ss->tcb.vm;
  if (sandbox_vm->exception.tt == MRBC_TT_NIL) {
    SET_NIL_RETURN();
  } else {
    SET_RETURN(sandbox_vm->exception);
  }
}

static void
c_sandbox_result(mrb_vm *vm, mrb_value *v, int argc)
{
  SS();
  mrbc_vm *sandbox_vm = (mrbc_vm *)&ss->tcb.vm;
  //if (sandbox_vm->regs[ss->p->scope->sp].tt == MRBC_TT_EMPTY) {
  //  // fallback but FIXME
  //  SET_NIL_RETURN();
  //} else {
    SET_RETURN(sandbox_vm->regs[ss->p->scope->sp]);
  //}
}

static void
c_sandbox_suspend(mrb_vm *vm, mrb_value *v, int argc)
{
  SS();
  mrbc_suspend_task(&ss->tcb);
  { /*
       Workaround but causes memory leak 😔
       To preserve symbol table
    */
    if (ss->p->scope->vm_code) ss->p->scope->vm_code = NULL;
  }
  Compiler_parserStateFree(ss->p);
}

static void
c_sandbox_compile(mrb_vm *vm, mrb_value *v, int argc)
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

#define ZERO     "\e[30;1m"
#define CTRL     "\e[35;1m"
#define LETTER   "\e[36m"
#define EIGHTBIT "\e[33m"
void c_tud_task(mrbc_vm *vm, mrbc_value v[], int argc);
static void dump(Scope *scope, mrb_vm *vm, mrb_value *v)
{
  int c, i, j;
  for (i = 0; i < scope->vm_code_size; i++) {
    c = scope->vm_code[i];
    if (i != 0) {
      if (i % 16 == 0) {
        console_printf("\n");
      } else if (i % 8 == 0) {
        console_printf("| ");
      }
      c_tud_task(vm, v, 0);
    }
    if (c == 0) {
      console_printf(ZERO);
    } else if (c < 0x20) {
      console_printf(CTRL);
    } else if (c < 0x7f) {
      console_printf(LETTER);
    } else {
      console_printf(EIGHTBIT);
    }
    c_tud_task(vm, v, 0);
    console_printf("%02x\e[m ", c);
    c_tud_task(vm, v, 0);
  }
  console_printf("\n");
  c_tud_task(vm, v, 0);
}

static void
c_sandbox_resume(mrb_vm *vm, mrb_value *v, int argc)
{
  SS();
  mrbc_vm *sandbox_vm = (mrbc_vm *)&ss->tcb.vm;
  if (sandbox_vm->vm_id == 7) {
    volatile int ii;
    ii++;
  }
  //dump(ss->p->scope, vm, v);
  if(mrbc_load_mrb(sandbox_vm, ss->p->scope->vm_code) != 0) {
    Compiler_parserStateFree(ss->p);
    SET_FALSE_RETURN();
  } else {
    sandbox_vm->cur_irep = sandbox_vm->top_irep;
    sandbox_vm->inst = sandbox_vm->cur_irep->inst;
    sandbox_vm->callinfo_tail = NULL;
    sandbox_vm->target_class = mrbc_class_object;
    sandbox_vm->flag_preemption = 0;
    mrbc_resume_task(&ss->tcb);
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
c_sandbox_new(mrb_vm *vm, mrb_value *v, int argc)
{
  mrbc_value sandbox = mrbc_instance_new(vm, v->cls, sizeof(SandboxState));
  SandboxState *ss = (SandboxState *)sandbox.instance->data;
  mrbc_init_tcb(&ss->tcb);
  mrbc_create_task(sandbox_task, &ss->tcb);
  ss->tcb.vm.flag_permanence = 1;
  picorbc_context_new(&ss->cxt);
  SET_RETURN(sandbox);
}

void
mrbc_sandbox_init(void)
{
  mrbc_class *mrbc_class_Sandbox = mrbc_define_class(0, "Sandbox", mrbc_class_object);
  mrbc_define_method(0, mrbc_class_Sandbox, "compile", c_sandbox_compile);
  mrbc_define_method(0, mrbc_class_Sandbox, "resume",  c_sandbox_resume);
  mrbc_define_method(0, mrbc_class_Sandbox, "state",   c_sandbox_state);
  mrbc_define_method(0, mrbc_class_Sandbox, "result",  c_sandbox_result);
  mrbc_define_method(0, mrbc_class_Sandbox, "error",   c_sandbox_error);
  mrbc_define_method(0, mrbc_class_Sandbox, "suspend", c_sandbox_suspend);
  mrbc_define_method(0, mrbc_class_Sandbox, "new",     c_sandbox_new);
}

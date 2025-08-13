#include <mrc_common.h>
#include <mrc_ccontext.h>
#include <mrc_compile.h>
#include <mrc_dump.h>
#include <mrubyc.h>


static void
c_kernel_eval(mrbc_vm *vm, mrbc_value *v, int argc)
{
  char *utf8 = (char *)GET_STRING_ARG(1);
  mrc_ccontext *c = mrc_ccontext_new(NULL);
  mrc_irep *irep = mrc_load_string_cxt(c, (const uint8_t **)&utf8, strlen(utf8));
  if (irep == NULL) {
    goto SYNTAX_ERROR;
  }
  int result;
  uint8_t *mrb = NULL;
  size_t mrb_size = 0;
  result = mrc_dump_irep(c, irep, 0, &mrb, &mrb_size);
  if (result != MRC_DUMP_OK) {
    goto SYNTAX_ERROR;
  }
  mrc_irep_free(c, irep);
  mrc_ccontext_free(c);
  mrbc_tcb *tcb = mrbc_create_task(mrb, NULL);
  tcb->vm.flag_preemption = 0;
  SET_NIL_RETURN();
  return;
SYNTAX_ERROR:
  if (irep) mrc_irep_free(c, irep);
  mrc_ccontext_free(c);
  mrbc_class *SyntaxError = mrbc_get_class_by_name("SyntaxError");
  mrbc_raisef(vm, (struct RClass *)SyntaxError, "eval string syntax error: %s", utf8);
  return;
}

void
mrbc_eval_init(mrbc_vm *vm)
{
  mrbc_class *module_Kernel = mrbc_get_class_by_name("Kernel");
  mrbc_define_method(vm, module_Kernel, "eval", c_kernel_eval);

  mrbc_define_class(vm, "SyntaxError", mrbc_get_class_by_name("Exception"));
}

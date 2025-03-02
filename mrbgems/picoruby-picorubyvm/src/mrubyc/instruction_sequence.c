#include <mrubyc.h>

static void
c_compile(struct VM *vm, mrbc_value v[], int argc)
{
  if (v->tt != MRBC_TT_CLASS) {
    mrbc_raise(vm, MRBC_CLASS(NoMethodError), "undefined method 'compile' for instance");
    return;
  }
  mrc_ccontext *c = mrc_ccontext_new(NULL);
  uint8_t *code = GET_STRING_ARG(1);
  size_t code_size = strlen((const char *)code);
  mrc_irep *irep = mrc_load_string_cxt(c, (const uint8_t **)&code, code_size);
  if (irep) mrc_irep_remove_lv(c, irep);

  uint8_t *vm_code = NULL;
  size_t vm_code_size = 0;
  int result = mrc_dump_irep(c, (const mrc_irep *)irep, 0, &vm_code, &vm_code_size);
  if (result != MRC_DUMP_OK) {
    mrc_ccontext_free(c);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Dump failed");
    return;
  }
  mrbc_value iseq = mrbc_instance_new(vm, v->cls, sizeof(iseq_binary) + vm_code_size);
  iseq_binary *binary = (iseq_binary *)iseq.instance->data;
  binary->size = vm_code_size;
  memcpy(binary->data, vm_code, vm_code_size);
  mrbc_free(vm, vm_code);
  SET_RETURN(iseq);
  mrc_irep_free(c, irep);
  mrc_ccontext_free(c);
}

static void
c_to_binary(struct VM *vm, mrbc_value v[], int argc)
{
  iseq_binary *binary = (iseq_binary *)v->instance->data;
  SET_RETURN(mrbc_string_new(vm, binary->data, binary->size));
}

static void
c_eval(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "Not implemented");
}

void
mrbc_instruction_sequence_init(mrbc_vm *vm, mrbc_class *class_PicoRubyVM)
{
   = mrbc_define_class(vm, "PicoRubyVM", mrbc_class_object);
  mrbc_class *class_InstructionSequence = mrbc_define_class_under(0, class_PicoRubyVM, "InstructionSequence", mrbc_class_object);
  mrbc_define_method(vm, class_InstructionSequence, "compile", c_compile);
  mrbc_define_method(vm, class_InstructionSequence, "to_binary", c_to_binary);
  mrbc_define_method(vm, class_InstructionSequence, "eval", c_eval);
}


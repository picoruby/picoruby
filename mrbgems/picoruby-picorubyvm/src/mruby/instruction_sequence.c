#include <mruby.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/presym.h>

static void mrb_iseq_binary_free(mrb_state *mrb, void *ptr) {
  /* custom destructor */
  mrb_free(mrb, ptr);
}
struct mrb_data_type mrb_iseq_binary_type = { "InstructionSequence", mrb_iseq_binary_free };


static mrb_value
mrb_instruction_sequence_initialize(mrb_state *mrb, mrb_value self)
{
  return self;
}

static mrb_value
mrb_instruction_sequence_s_compile(mrb_state *mrb, mrb_value klass)
{
  mrb_value self = mrb_obj_new(mrb, mrb_class_ptr(klass), 0, NULL);


  uint32_t kw_num = 1;
  uint32_t kw_required = 0;
  mrb_sym kw_names[] = { MRB_SYM(remove_lv) };
  mrb_value kw_values[kw_num];
  mrb_kwargs kwargs = { kw_num, kw_required, kw_names, kw_values, NULL };
  if (mrb_undef_p(kw_values[0])) { kw_values[0] = mrb_true_value(); }

  const char *code;
  mrb_get_args(mrb, "z:", &code, &kwargs);
  size_t code_size = strlen(code);

  mrc_ccontext *c = mrc_ccontext_new(mrb);
  if (c == NULL) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Failed to create compile context");
  }
  mrc_irep *irep = mrc_load_string_cxt(c, (const uint8_t **)&code, code_size);
  if (irep == NULL) {
    mrc_ccontext_free(c);
    mrb_raise(mrb, E_RUNTIME_ERROR, "Failed to compile instruction sequence");
  }
  else if (mrb_test(kw_values[0])) {
    mrc_irep_remove_lv(c, irep);
  }

  uint8_t *vm_code = NULL;
  size_t vm_code_size = 0;
  mrb_int result = mrc_dump_irep(c, (const mrc_irep *)irep, 0, &vm_code, &vm_code_size);
  if (result != MRC_DUMP_OK) {
    mrc_irep_free(c, irep);
    mrc_ccontext_free(c);
    mrb_raise(mrb, E_RUNTIME_ERROR, "Failed to compile instruction sequence");
  }
  iseq_binary *binary = (iseq_binary *)mrb_malloc(mrb, sizeof(iseq_binary) + vm_code_size);
  if (binary == NULL) {
    mrc_ccontext_free(c);
    mrb_raise(mrb, E_RUNTIME_ERROR, "Failed to allocate memory");
  }
  DATA_PTR(self) = binary;
  DATA_TYPE(self) = &mrb_iseq_binary_type;

  binary->size = vm_code_size;
  memcpy(binary->data, vm_code, vm_code_size);
  mrb_free(mrb, vm_code);
  mrc_irep_free(c, irep);
  mrc_ccontext_free(c);

  return self;
}

static mrb_value
mrb_instruction_sequence_to_binary(mrb_state *mrb, mrb_value self)
{
  iseq_binary *binary;
  binary = (iseq_binary *)mrb_data_get_ptr(mrb, self, &mrb_iseq_binary_type);
  if (binary == NULL) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "uninitialized iseq_binary data");
  }

  return mrb_str_new(mrb, (const char *)binary->data, binary->size);
}

static mrb_value
mrb_instruction_sequence_s_eval(mrb_state *mrb, mrb_value klass)
{
  mrb_notimplement(mrb);
  return mrb_nil_value();
}

void
mrb_instruction_sequence_init(mrb_state *mrb, struct RClass *class_PicoRubyVM)
{
  struct RClass *class_InstructionSequence = mrb_define_class_under_id(mrb, class_PicoRubyVM, MRB_SYM(InstructionSequence), mrb->object_class);

  MRB_SET_INSTANCE_TT(class_InstructionSequence, MRB_TT_CDATA);

  mrb_define_class_method_id(mrb, class_InstructionSequence, MRB_SYM(compile), mrb_instruction_sequence_s_compile, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_InstructionSequence, MRB_SYM(initialize), mrb_instruction_sequence_initialize, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_InstructionSequence, MRB_SYM(to_binary), mrb_instruction_sequence_to_binary, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_InstructionSequence, MRB_SYM(eval), mrb_instruction_sequence_s_eval, MRB_ARGS_REQ(1));
}

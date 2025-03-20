#include <mruby.h>
#include <mruby/presym.h>
#include "tlsf_utils.h"

static mrb_value
mrb_picorubyvm_s_memory_statistics(mrb_state *mrb, mrb_value klass)
{
  return mrb_tlsf_statistics(mrb);
}

static mrb_value
mrb_picorubyvm_s_alloc_start_profiling(mrb_state *mrb, mrb_value klass)
{
  mrb_notimplement(mrb);
  return mrb_nil_value();
}

static mrb_value
mrb_picorubyvm_s_alloc_stop_profiling(mrb_state *mrb, mrb_value klass)
{
  mrb_notimplement(mrb);
  return mrb_nil_value();
}

static mrb_value
mrb_picorubyvm_s_alloc_profiling_result(mrb_state *mrb, mrb_value klass)
{
  mrb_notimplement(mrb);
  return mrb_nil_value();
}


void
mrb_picoruby_picorubyvm_gem_init(mrb_state *mrb)
{
  struct RClass *class_PicoRubyVM = mrb_define_class_id(mrb, MRB_SYM(PicoRubyVM), mrb->object_class);

  mrb_define_class_method_id(mrb, class_PicoRubyVM, MRB_SYM(memory_statistics), mrb_picorubyvm_s_memory_statistics, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_PicoRubyVM, MRB_SYM(alloc_start_profiling), mrb_picorubyvm_s_alloc_start_profiling, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_PicoRubyVM, MRB_SYM(alloc_stop_profiling), mrb_picorubyvm_s_alloc_stop_profiling, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_PicoRubyVM, MRB_SYM(alloc_profiling_result), mrb_picorubyvm_s_alloc_profiling_result, MRB_ARGS_NONE());

  mrb_instruction_sequence_init(mrb, class_PicoRubyVM);
}

void
mrb_picoruby_picorubyvm_gem_final(mrb_state* mrb)
{
}

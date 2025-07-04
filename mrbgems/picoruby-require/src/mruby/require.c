#include <mruby.h>
#include <mruby/presym.h>

static mrb_value
mrb_extern(mrb_state *mrb, mrb_value self)
{
  return mrb_nil_value();
}

void
mrb_picoruby_require_gem_init(mrb_state* mrb)
{
  struct RClass *module_Kernel = mrb_define_module_id(mrb, MRB_SYM(Kernel));

  mrb_define_method_id(mrb, module_Kernel, MRB_SYM(extern), mrb_extern, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_require_gem_final(mrb_state* mrb)
{
}


#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/error.h>
#include <mruby/string.h>
#include <mruby/variable.h>
#include <mruby/presym.h>
#include <string.h>

extern const char *prebuilt_gems[];

static mrb_value
mrb_extern(mrb_state *mrb, mrb_value self)
{
  char *name;
  mrb_bool force;
  mrb_get_args(mrb, "z|b", &name, &force);
  for (int i = 0; prebuilt_gems[i]; i++) {
    if (strcmp(prebuilt_gems[i], name) == 0) {
      return mrb_false_value();
    }
  }
  return mrb_nil_value();
}

void
mrb_picoruby_require_gem_init(mrb_state* mrb)
{
  struct RClass *module_Kernel = mrb_define_module_id(mrb, MRB_SYM(Kernel));

  mrb_define_private_method_id(mrb, module_Kernel, MRB_SYM(extern), mrb_extern, MRB_ARGS_ARG(1,1));

  mrb_value loaded_features = mrb_ary_new(mrb);
  mrb_gv_set(mrb, MRB_GVSYM(LOADED_FEATURES), loaded_features);
  for (int i = 0; prebuilt_gems[i]; i++) {
    mrb_ary_push(mrb, loaded_features, mrb_str_new_cstr(mrb, prebuilt_gems[i]));
  }
}

void
mrb_picoruby_require_gem_final(mrb_state* mrb)
{
}

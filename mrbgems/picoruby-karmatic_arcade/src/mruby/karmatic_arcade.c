#include "bdffont_mruby.h"

static mrb_value
mrb_s_10(mrb_state *mrb, mrb_value self)
{
  const char *text;
  mrb_int scale = 1;
  mrb_get_args(mrb, "z|i", &text, &scale);
  return bdffont_render(mrb, text, scale,
                        KARMATIC_ARCADE_HEIGHT,
                        array_of_karmatic_arcade_sub);
}

void
mrb_picoruby_karmatic_arcade_gem_init(mrb_state *mrb)
{
  struct RClass *mod = mrb_define_module_id(mrb, MRB_SYM(KarmaticArcade));
  mrb_define_class_method_id(mrb, mod, MRB_SYM(_10), mrb_s_10, MRB_ARGS_ARG(1, 1));
}

void
mrb_picoruby_karmatic_arcade_gem_final(mrb_state *mrb)
{
}

#include "bdffont_mruby.h"

static mrb_value
mrb_s_6x12(mrb_state *mrb, mrb_value self)
{
  const char *text;
  mrb_int scale = 1;
  mrb_get_args(mrb, "z|i", &text, &scale);
  return bdffont_render(mrb, text, scale, 12, array_of_terminus_6x12);
}

static mrb_value
mrb_s_8x16(mrb_state *mrb, mrb_value self)
{
  const char *text;
  mrb_int scale = 1;
  mrb_get_args(mrb, "z|i", &text, &scale);
  return bdffont_render(mrb, text, scale, 16, array_of_terminus_8x16);
}

static mrb_value
mrb_s_12x24(mrb_state *mrb, mrb_value self)
{
  const char *text;
  mrb_int scale = 1;
  mrb_get_args(mrb, "z|i", &text, &scale);
  return bdffont_render(mrb, text, scale, 24, array_of_terminus_12x24);
}

static mrb_value
mrb_s_16x32(mrb_state *mrb, mrb_value self)
{
  const char *text;
  mrb_int scale = 1;
  mrb_get_args(mrb, "z|i", &text, &scale);
  return bdffont_render(mrb, text, scale, 32, array_of_terminus_16x32);
}

void
mrb_picoruby_terminus_gem_init(mrb_state *mrb)
{
  struct RClass *module_Terminus = mrb_define_module_id(mrb, MRB_SYM(Terminus));

  mrb_define_class_method_id(mrb, module_Terminus, MRB_SYM(_6x12),  mrb_s_6x12,  MRB_ARGS_ARG(1, 1));
  mrb_define_class_method_id(mrb, module_Terminus, MRB_SYM(_8x16),  mrb_s_8x16,  MRB_ARGS_ARG(1, 1));
  mrb_define_class_method_id(mrb, module_Terminus, MRB_SYM(_12x24), mrb_s_12x24, MRB_ARGS_ARG(1, 1));
  mrb_define_class_method_id(mrb, module_Terminus, MRB_SYM(_16x32), mrb_s_16x32, MRB_ARGS_ARG(1, 1));
}

void
mrb_picoruby_terminus_gem_final(mrb_state *mrb)
{
}

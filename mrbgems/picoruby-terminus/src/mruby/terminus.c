#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/string.h>
#include <mruby/array.h>

static mrb_value
array_of_terminus(mrb_state *mrb, int w, int h, bool (*array_func)(const char **, uint64_t *))
{
  mrb_value result = mrb_ary_new(mrb);
  mrb_value widths = mrb_ary_new(mrb);
  mrb_int total_width = 0;
  mrb_value glyphs = mrb_ary_new(mrb);
  const char *text;
  mrb_get_args(mrb, "z", &text);

  const char *p = text;
  while (*p) {
    uint64_t lines[h + 1] __attribute__((aligned(8)));
    if (!array_func(&p, lines)) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "Invalid character (ASCII only)");
    }

    mrb_value ch = mrb_ary_new_capa(mrb, h);
    total_width += w;
    mrb_ary_push(mrb, widths, mrb_fixnum_value(w));

    for (int i = 1; i <= h; i++) {
      mrb_ary_push(mrb, ch, mrb_int_value(mrb, lines[i]));
    }
    mrb_ary_push(mrb, glyphs, ch);
  }

  mrb_ary_push(mrb, result, mrb_fixnum_value(h));
  mrb_ary_push(mrb, result, mrb_fixnum_value(total_width));
  mrb_ary_push(mrb, result, widths);
  mrb_ary_push(mrb, result, glyphs);
  return result;
}

static mrb_value
mrb_s_6x12(mrb_state *mrb, mrb_value self)
{
  return array_of_terminus(mrb, 6, 12, array_of_terminus_6x12);
}

static mrb_value
mrb_s_8x16(mrb_state *mrb, mrb_value self)
{
  return array_of_terminus(mrb, 8, 16, array_of_terminus_8x16);
}

static mrb_value
mrb_s_12x24(mrb_state *mrb, mrb_value self)
{
  return array_of_terminus(mrb, 12, 24, array_of_terminus_12x24);
}

static mrb_value
mrb_s_16x32(mrb_state *mrb, mrb_value self)
{
  return array_of_terminus(mrb, 16, 32, array_of_terminus_16x32);
}

void
mrb_picoruby_terminus_gem_init(mrb_state* mrb)
{
  struct RClass *module_Terminus = mrb_define_module_id(mrb, MRB_SYM(Terminus));

  mrb_define_class_method_id(mrb, module_Terminus, MRB_SYM(_6x12), mrb_s_6x12, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, module_Terminus, MRB_SYM(_8x16), mrb_s_8x16, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, module_Terminus, MRB_SYM(_12x24), mrb_s_12x24, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, module_Terminus, MRB_SYM(_16x32), mrb_s_16x32, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_terminus_gem_final(mrb_state* mrb)
{
}
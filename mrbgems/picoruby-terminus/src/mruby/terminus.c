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
  mrb_int scale = 1;
  mrb_get_args(mrb, "z|i", &text, &scale);

  if (scale < 1 || 4 < scale) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Invalid scale. Expect 1..4");
  }

  mrb_int scaled_w = w * scale;
  mrb_int scaled_h = h * scale;

  const char *p = text;
  while (*p) {
    uint64_t lines[h + 1] __attribute__((aligned(8)));
    if (!array_func(&p, lines)) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "Invalid character (ASCII only)");
    }

    uint64_t output[scaled_h] __attribute__((aligned(8)));

    if (1 < scale) {
      for (int i = 1; i <= h; i++) {
        lines[i] = expand_bits(lines[i], w, scale);
      }
      for (int i = 0; i < h; i++) {
        for (int j = 0; j < scale; j++) {
          output[i * scale + j] = lines[i + 1];
        }
      }
      smooth_edges(output, scaled_w, scaled_h);
    }

    total_width += scaled_w;

    if (32 < scaled_w) {
      mrb_ary_push(mrb, widths, mrb_fixnum_value(scaled_w - 32));
      mrb_ary_push(mrb, widths, mrb_fixnum_value(32));
    } else {
      mrb_ary_push(mrb, widths, mrb_fixnum_value(scaled_w));
    }

    mrb_value ch = mrb_ary_new_capa(mrb, scaled_h);

    if (scale == 1) {
      for (int i = 1; i <= h; i++) {
        mrb_ary_push(mrb, ch, mrb_int_value(mrb, lines[i]));
      }
    } else {
      for (int i = 0; i < scaled_h; i++) {
        if (32 < scaled_w) {
          mrb_ary_push(mrb, ch, mrb_int_value(mrb, (mrb_int)(uint32_t)(output[i] >> 32)));
        } else {
          mrb_ary_push(mrb, ch, mrb_int_value(mrb, (mrb_int)(uint32_t)output[i]));
        }
      }
    }

    mrb_ary_push(mrb, glyphs, ch);

    if (32 < scaled_w) {
      mrb_value ch2 = mrb_ary_new_capa(mrb, scaled_h);
      for (int i = 0; i < scaled_h; i++) {
        mrb_ary_push(mrb, ch2, mrb_int_value(mrb, (mrb_int)(uint32_t)output[i]));
      }
      mrb_ary_push(mrb, glyphs, ch2);
    }
  }

  mrb_ary_push(mrb, result, mrb_fixnum_value(scaled_h));
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

  mrb_define_class_method_id(mrb, module_Terminus, MRB_SYM(_6x12), mrb_s_6x12, MRB_ARGS_ARG(1,1));
  mrb_define_class_method_id(mrb, module_Terminus, MRB_SYM(_8x16), mrb_s_8x16, MRB_ARGS_ARG(1,1));
  mrb_define_class_method_id(mrb, module_Terminus, MRB_SYM(_12x24), mrb_s_12x24, MRB_ARGS_ARG(1,1));
  mrb_define_class_method_id(mrb, module_Terminus, MRB_SYM(_16x32), mrb_s_16x32, MRB_ARGS_ARG(1,1));
}

void
mrb_picoruby_terminus_gem_final(mrb_state* mrb)
{
}
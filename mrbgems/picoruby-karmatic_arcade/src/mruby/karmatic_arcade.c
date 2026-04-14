#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/string.h>
#include <mruby/array.h>

static mrb_value
mrb_s_20(mrb_state *mrb, mrb_value self)
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

  mrb_int scaled_h = KARMATIC_ARCADE_HEIGHT * scale;
  const char *p = text;

  while (*p) {
    uint64_t lines[KARMATIC_ARCADE_HEIGHT + 1] __attribute__((aligned(8)));
    if (!array_of_karmatic_arcade_sub(&p, lines)) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "Invalid character (ASCII only)");
    }

    int w          = (int)lines[0];
    mrb_int scaled_w = w * scale;

    uint64_t output[scaled_h] __attribute__((aligned(8)));

    if (1 < scale) {
      for (int i = 1; i <= KARMATIC_ARCADE_HEIGHT; i++) {
        lines[i] = expand_bits(lines[i], w, scale);
      }
      for (int i = 0; i < KARMATIC_ARCADE_HEIGHT; i++) {
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
      for (int i = 1; i <= KARMATIC_ARCADE_HEIGHT; i++) {
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

void
mrb_picoruby_karmatic_arcade_gem_init(mrb_state *mrb)
{
  struct RClass *mod = mrb_define_module(mrb, "KarmaticArcade");
  mrb_define_class_method_id(mrb, mod, MRB_SYM(_20), mrb_s_20, MRB_ARGS_ARG(1, 1));
}

void
mrb_picoruby_karmatic_arcade_gem_final(mrb_state *mrb)
{
}

#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/string.h>
#include <mruby/array.h>

mrb_value
bdffont_render(mrb_state *mrb, const char *text,
               mrb_int scale, int cell_h, bdffont_glyph_fn fn)
{
  if (scale < 1 || 4 < scale) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Invalid scale. Expect 1..4");
  }

  mrb_value result = mrb_ary_new(mrb);
  mrb_value widths = mrb_ary_new(mrb);
  mrb_int total_width = 0;
  mrb_value glyphs = mrb_ary_new(mrb);
  mrb_int scaled_h = (mrb_int)(cell_h * scale);

  const char *p = text;
  while (*p) {
    uint64_t lines[cell_h + 1] __attribute__((aligned(8)));
    if (!fn(&p, lines)) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "Invalid character");
    }

    mrb_int cell_w  = (mrb_int)lines[0];
    mrb_int scaled_w = cell_w * scale;

    uint64_t output[scaled_h] __attribute__((aligned(8)));

    if (1 < scale) {
      int i = 0;
      while (i < cell_h) {
        lines[i + 1] = bdffont_expand_bits(lines[i + 1], (int)cell_w, scale);
        i++;
      }
      i = 0;
      while (i < cell_h) {
        int j = 0;
        while (j < scale) {
          output[i * scale + j] = lines[i + 1];
          j++;
        }
        i++;
      }
      bdffont_smooth_edges(output, (int)scaled_w, (int)scaled_h);
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
      int i = 1;
      while (i <= cell_h) {
        mrb_ary_push(mrb, ch, mrb_int_value(mrb, (mrb_int)lines[i]));
        i++;
      }
    } else {
      int i = 0;
      while (i < (int)scaled_h) {
        if (32 < scaled_w) {
          mrb_ary_push(mrb, ch, mrb_int_value(mrb, (mrb_int)(uint32_t)(output[i] >> 32)));
        } else {
          mrb_ary_push(mrb, ch, mrb_int_value(mrb, (mrb_int)(uint32_t)output[i]));
        }
        i++;
      }
    }

    mrb_ary_push(mrb, glyphs, ch);

    if (32 < scaled_w) {
      mrb_value ch2 = mrb_ary_new_capa(mrb, scaled_h);
      int i = 0;
      while (i < (int)scaled_h) {
        mrb_ary_push(mrb, ch2, mrb_int_value(mrb, (mrb_int)(uint32_t)output[i]));
        i++;
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
mrb_picoruby_bdffont_gem_init(mrb_state *mrb)
{
  mrb_define_module_id(mrb, MRB_SYM(BDFFont));
}

void
mrb_picoruby_bdffont_gem_final(mrb_state *mrb)
{
}

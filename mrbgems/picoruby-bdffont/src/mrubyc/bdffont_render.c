#include <mrubyc.h>

mrbc_value
bdffont_render(mrbc_vm *vm, const char *text,
               mrbc_int_t scale, int cell_h, bdffont_glyph_fn fn)
{
  if (scale < 1 || 4 < scale) {
    return mrbc_nil_value();
  }

  mrbc_value result = mrbc_array_new(vm, 4);
  mrbc_value widths = mrbc_array_new(vm, 0);
  mrbc_int_t total_width = 0;
  mrbc_value glyphs = mrbc_array_new(vm, 0);
  mrbc_int_t scaled_h = (mrbc_int_t)(cell_h * scale);

  const char *p = text;
  while (*p) {
    uint64_t lines[cell_h + 1] __attribute__((aligned(8)));
    if (!fn(&p, lines)) {
      return mrbc_nil_value();
    }

    mrbc_int_t cell_w  = (mrbc_int_t)lines[0];
    mrbc_int_t scaled_w = cell_w * scale;

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
      mrbc_value val1 = mrbc_integer_value(scaled_w - 32);
      mrbc_value val2 = mrbc_integer_value(32);
      mrbc_array_push(&widths, &val1);
      mrbc_array_push(&widths, &val2);
    } else {
      mrbc_value val = mrbc_integer_value(scaled_w);
      mrbc_array_push(&widths, &val);
    }

    mrbc_value ch = mrbc_array_new(vm, scaled_h);

    if (scale == 1) {
      int i = 1;
      while (i <= cell_h) {
        mrbc_value val = mrbc_integer_value((mrbc_int_t)lines[i]);
        mrbc_array_push(&ch, &val);
        i++;
      }
    } else {
      int i = 0;
      while (i < (int)scaled_h) {
        if (32 < scaled_w) {
          mrbc_value val = mrbc_integer_value((mrbc_int_t)(uint32_t)(output[i] >> 32));
          mrbc_array_push(&ch, &val);
        } else {
          mrbc_value val = mrbc_integer_value((mrbc_int_t)(uint32_t)output[i]);
          mrbc_array_push(&ch, &val);
        }
        i++;
      }
    }

    mrbc_array_push(&glyphs, &ch);

    if (32 < scaled_w) {
      mrbc_value ch2 = mrbc_array_new(vm, scaled_h);
      int i = 0;
      while (i < (int)scaled_h) {
        mrbc_value val = mrbc_integer_value((mrbc_int_t)(uint32_t)output[i]);
        mrbc_array_push(&ch2, &val);
        i++;
      }
      mrbc_array_push(&glyphs, &ch2);
    }
  }

  mrbc_value height_val      = mrbc_integer_value(scaled_h);
  mrbc_value total_width_val = mrbc_integer_value(total_width);
  mrbc_array_push(&result, &height_val);
  mrbc_array_push(&result, &total_width_val);
  mrbc_array_push(&result, &widths);
  mrbc_array_push(&result, &glyphs);

  return result;
}

void
mrbc_bdffont_init(mrbc_vm *vm)
{
  mrbc_define_module(vm, "BDFFont");
}

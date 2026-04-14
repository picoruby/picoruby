#include <mrubyc.h>

static void
c_s_20(mrbc_vm *vm, mrbc_value *v, int argc)
{
  const char *text = (const char *)GET_STRING_ARG(1);
  mrbc_int_t scale = (argc > 1) ? GET_INT_ARG(2) : 1;

  if (scale < 1 || 4 < scale) {
    SET_RETURN(mrbc_nil_value());
    return;
  }

  mrbc_value result      = mrbc_array_new(vm, 4);
  mrbc_value widths      = mrbc_array_new(vm, 0);
  mrbc_int_t total_width = 0;
  mrbc_value glyphs      = mrbc_array_new(vm, 0);
  mrbc_int_t scaled_h    = KARMATIC_ARCADE_HEIGHT * scale;

  const char *p = text;
  while (*p) {
    uint64_t lines[KARMATIC_ARCADE_HEIGHT + 1] __attribute__((aligned(8)));
    if (!array_of_karmatic_arcade_sub(&p, lines)) {
      SET_RETURN(mrbc_nil_value());
      return;
    }

    int        w        = (int)lines[0];
    mrbc_int_t scaled_w = w * scale;

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
      for (int i = 1; i <= KARMATIC_ARCADE_HEIGHT; i++) {
        mrbc_value val = mrbc_integer_value(lines[i]);
        mrbc_array_push(&ch, &val);
      }
    } else {
      for (int i = 0; i < scaled_h; i++) {
        if (32 < scaled_w) {
          mrbc_value val = mrbc_integer_value((mrbc_int_t)(uint32_t)(output[i] >> 32));
          mrbc_array_push(&ch, &val);
        } else {
          mrbc_value val = mrbc_integer_value((mrbc_int_t)(uint32_t)output[i]);
          mrbc_array_push(&ch, &val);
        }
      }
    }

    mrbc_array_push(&glyphs, &ch);

    if (32 < scaled_w) {
      mrbc_value ch2 = mrbc_array_new(vm, scaled_h);
      for (int i = 0; i < scaled_h; i++) {
        mrbc_value val = mrbc_integer_value((mrbc_int_t)(uint32_t)output[i]);
        mrbc_array_push(&ch2, &val);
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

  SET_RETURN(result);
}

void
mrbc_karmatic_arcade_init(mrbc_vm *vm)
{
  mrbc_class *mod = mrbc_define_module(vm, "KarmaticArcade");
  mrbc_define_method(vm, mod, "_20", c_s_20);
}

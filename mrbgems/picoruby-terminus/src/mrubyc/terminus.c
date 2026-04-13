#include <mrubyc.h>

static mrbc_value
array_of_terminus(mrbc_vm *vm, mrbc_value *v, int argc, int w, int h, bool (*array_func)(const char **, uint64_t *))
{
  const char *text = (const char *)GET_STRING_ARG(1);
  mrbc_int_t scale = (argc > 1) ? GET_INT_ARG(2) : 1;

  if (scale < 1 || 4 < scale) {
    return mrbc_nil_value();
  }

  mrbc_value result = mrbc_array_new(vm, 4);
  mrbc_value widths = mrbc_array_new(vm, 0);
  mrbc_int_t total_width = 0;
  mrbc_value glyphs = mrbc_array_new(vm, 0);
  mrbc_int_t scaled_w = w * scale;
  mrbc_int_t scaled_h = h * scale;

  const char *p = text;
  while (*p) {
    uint64_t lines[h + 1] __attribute__((aligned(8)));
    if (!array_func(&p, lines)) {
      return mrbc_nil_value();
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
      for (int i = 1; i <= h; i++) {
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

  mrbc_value height_val = mrbc_integer_value(scaled_h);
  mrbc_value total_width_val = mrbc_integer_value(total_width);
  mrbc_array_push(&result, &height_val);
  mrbc_array_push(&result, &total_width_val);
  mrbc_array_push(&result, &widths);
  mrbc_array_push(&result, &glyphs);

  return result;
}

static void
c_s_6x12(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value result = array_of_terminus(vm, v, argc, 6, 12, array_of_terminus_6x12);
  SET_RETURN(result);
}

static void
c_s_8x16(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value result = array_of_terminus(vm, v, argc, 8, 16, array_of_terminus_8x16);
  SET_RETURN(result);
}

static void
c_s_12x24(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value result = array_of_terminus(vm, v, argc, 12, 24, array_of_terminus_12x24);
  SET_RETURN(result);
}

static void
c_s_16x32(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value result = array_of_terminus(vm, v, argc, 16, 32, array_of_terminus_16x32);
  SET_RETURN(result);
}

void
mrbc_terminus_init(mrbc_vm *vm)
{
  mrbc_class *module_Terminus = mrbc_define_module(vm, "Terminus");

  mrbc_define_method(vm, module_Terminus, "_6x12", c_s_6x12);
  mrbc_define_method(vm, module_Terminus, "_8x16", c_s_8x16);
  mrbc_define_method(vm, module_Terminus, "_12x24", c_s_12x24);
  mrbc_define_method(vm, module_Terminus, "_16x32", c_s_16x32);
}
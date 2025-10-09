#include <mrubyc.h>

static mrbc_value
array_of_terminus(mrbc_vm *vm, mrbc_value *v, int argc, int w, int h, bool (*array_func)(const char **, uint64_t *))
{
  const char *text = (const char *)GET_STRING_ARG(1);

  mrbc_value result = mrbc_array_new(vm, 4);
  mrbc_value widths = mrbc_array_new(vm, 0);
  mrbc_int_t total_width = 0;
  mrbc_value glyphs = mrbc_array_new(vm, 0);

  const char *p = text;
  while (*p) {
    uint64_t lines[h + 1] __attribute__((aligned(8)));
    if (!array_func(&p, lines)) {
      return mrbc_nil_value();
    }

    mrbc_value ch = mrbc_array_new(vm, h);
    total_width += w;
    mrbc_value width_val = mrbc_integer_value(w);
    mrbc_array_push(&widths, &width_val);

    for (int i = 1; i <= h; i++) {
      mrbc_value val = mrbc_integer_value(lines[i]);
      mrbc_array_push(&ch, &val);
    }
    mrbc_array_push(&glyphs, &ch);
  }

  mrbc_value height_val = mrbc_integer_value(h);
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
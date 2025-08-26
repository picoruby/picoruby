#include <mrubyc.h>

static void
test_print(mrbc_vm *vm, mrbc_value *v, mrbc_int_t size)
{
  const char *text = (const char *)GET_STRING_ARG(1);
  const char *p = text;
  while (*p) {
    uint32_t c = utf8_to_unicode((const char **)&p);
    if (c < 0x80) {
      if (size == 12) {
        print_ascii_12((uint8_t)c);
      } else {
        print_ascii_16((uint8_t)c);
      }
    } else if (c < 0x10000) {
      uint16_t jis = unicode_to_jis(c);
      if (jis) {
        if (size == 12) {
          print_jis208_12(jis);
        } else {
          print_jis208_16(jis);
        }
      }
    } else {
      printf("unicode: %04X\n", c);
    }
  }
}

static void
c_s_test12(mrbc_vm *vm, mrbc_value *v, int argc)
{
  test_print(vm, v, 12);
  SET_NIL_RETURN();
}

static void
c_s_test16(mrbc_vm *vm, mrbc_value *v, int argc)
{
  test_print(vm, v, 16);
  SET_NIL_RETURN();
}

static mrbc_value
array_of_shinonome(mrbc_vm *vm, mrbc_value *v, int argc, mrbc_int_t size, const uint8_t *ascii_table, const uint8_t **jis208_table)
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
  mrbc_int_t height = size * scale;
  
  const char *p = text;
  while (*p) {
    uint64_t lines[size + 1] __attribute__((aligned(8)));
    if (!array_of_shinonome_sub(&p, lines, (size_t)size, ascii_table, jis208_table)) {
      return mrbc_nil_value();
    }
    
    lines[0] *= scale;
    uint64_t output[height] __attribute__((aligned(8)));
    
    if (1 < scale) {
      for (int i = 1; i < size + 1; i++) {
        lines[i] = expand_bits(lines[i], size, scale);
      }
      for (int i = 0; i < size; i++) {
        for (int j = 0; j < scale; j++) {
          output[i * scale + j] = lines[i + 1];
        }
      }
      smooth_edges(output, lines[0], height);
    }
    
    mrbc_value ch = mrbc_array_new(vm, height);
    total_width += lines[0];
    
    if (32 < lines[0]) {
      mrbc_value val1 = mrbc_integer_value(lines[0] - 32);
      mrbc_value val2 = mrbc_integer_value(32);
      mrbc_array_push(&widths, &val1);
      mrbc_array_push(&widths, &val2);
    } else {
      mrbc_value val = mrbc_integer_value(lines[0]);
      mrbc_array_push(&widths, &val);
    }
    
    if (scale == 1) {
      for (int i = 1; i < size + 1; i++) {
        mrbc_value val = mrbc_integer_value(lines[i]);
        mrbc_array_push(&ch, &val);
      }
    } else {
      for (int i = 0; i < height; i++) {
        if (32 < lines[0]) {
          mrbc_value val = mrbc_integer_value((mrbc_int_t)(uint32_t)(output[i]>>32));
          mrbc_array_push(&ch, &val);
        } else {
          mrbc_value val = mrbc_integer_value((mrbc_int_t)(uint32_t)output[i]);
          mrbc_array_push(&ch, &val);
        }
      }
    }
    
    mrbc_array_push(&glyphs, &ch);
    
    if (32 < lines[0]) {
      mrbc_value ch2 = mrbc_array_new(vm, height);
      for (int i = 0; i < height; i++) {
        mrbc_value val = mrbc_integer_value((mrbc_int_t)(uint32_t)output[i]);
        mrbc_array_push(&ch2, &val);
      }
      mrbc_array_push(&glyphs, &ch2);
    }
  }
  
  mrbc_value height_val = mrbc_integer_value(height);
  mrbc_value total_width_val = mrbc_integer_value(total_width);
  mrbc_array_push(&result, &height_val);
  mrbc_array_push(&result, &total_width_val);
  mrbc_array_push(&result, &widths);
  mrbc_array_push(&result, &glyphs);
  
  return result;
}

static void
c_s_maru12(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value result = array_of_shinonome(vm, v, argc, 12, ascii_12, jis208_12maru);
  SET_RETURN(result);
}

static void
c_s_go12(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value result = array_of_shinonome(vm, v, argc, 12, ascii_12, jis208_12go);
  SET_RETURN(result);
}

static void
c_s_min12(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value result = array_of_shinonome(vm, v, argc, 12, ascii_12, jis208_12min);
  SET_RETURN(result);
}

static void
c_s_go16(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value result = array_of_shinonome(vm, v, argc, 16, ascii_16, jis208_16go);
  SET_RETURN(result);
}

static void
c_s_min16(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value result = array_of_shinonome(vm, v, argc, 16, ascii_16, jis208_16min);
  SET_RETURN(result);
}

void
mrbc_shinonome_init(mrbc_vm *vm)
{
  mrbc_class *module_Shinonome = mrbc_define_module(vm, "Shinonome");
  
  mrbc_define_method(vm, module_Shinonome, "test12", c_s_test12);
  mrbc_define_method(vm, module_Shinonome, "test16", c_s_test16);
  mrbc_define_method(vm, module_Shinonome, "maru12", c_s_maru12);
  mrbc_define_method(vm, module_Shinonome, "go12", c_s_go12);
  mrbc_define_method(vm, module_Shinonome, "min12", c_s_min12);
  mrbc_define_method(vm, module_Shinonome, "go16", c_s_go16);
  mrbc_define_method(vm, module_Shinonome, "min16", c_s_min16);
}

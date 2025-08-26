#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/string.h>
#include <mruby/array.h>

static void
test_print(mrb_state *mrb, mrb_int size)
{
  const char *text;
  mrb_get_args(mrb, "z", &text);
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

static mrb_value
mrb_s_test12(mrb_state *mrb, mrb_value self)
{
  test_print(mrb, 12);
  return mrb_nil_value();
}

static mrb_value
mrb_s_test16(mrb_state *mrb, mrb_value self)
{
  test_print(mrb, 16);
  return mrb_nil_value();
}

static mrb_value
array_of_shinonome(mrb_state *mrb, mrb_int size, const uint8_t *ascii_table, const uint8_t **jis208_table)
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
  mrb_int height = size * scale;
  const char *p = text;
  while (*p) {
    uint64_t lines[size + 1] __attribute__((aligned(8)));
    if (!array_of_shinonome_sub(&p, lines, (size_t)size, ascii_table, jis208_table)) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "Invalid unicode");
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
    mrb_value ch = mrb_ary_new_capa(mrb, height + 1);
    total_width += lines[0];
    if (32 < lines[0]) {
      mrb_ary_push(mrb, widths, mrb_fixnum_value(lines[0] - 32));
      mrb_ary_push(mrb, widths, mrb_fixnum_value(32));
    } else {
      mrb_ary_push(mrb, widths, mrb_fixnum_value(lines[0]));
    }
    if (scale == 1) {
      for (int i = 1; i < size + 1; i++) {
        mrb_ary_push(mrb, ch, mrb_int_value(mrb, lines[i]));
      }
    } else {
      for (int i = 0; i < height; i++) {
        if (32 < lines[0]) {
          mrb_ary_push(mrb, ch, mrb_int_value(mrb, (mrb_int)(uint32_t)(output[i]>>32)));
        } else {
          mrb_ary_push(mrb, ch, mrb_int_value(mrb, (mrb_int)(uint32_t)output[i]));
        }
      }
    }
    mrb_ary_push(mrb, glyphs, ch);
    if (32 < lines[0]) {
      mrb_value ch2 = mrb_ary_new_capa(mrb, height + 1);
      for (int i = 0; i < height; i++) {
        mrb_ary_push(mrb, ch2, mrb_int_value(mrb, (mrb_int)(uint32_t)output[i]));
      }
      mrb_ary_push(mrb, glyphs, ch2);
    }
  }
  mrb_ary_push(mrb, result, mrb_fixnum_value(height));
  mrb_ary_push(mrb, result, mrb_fixnum_value(total_width));
  mrb_ary_push(mrb, result, widths);
  mrb_ary_push(mrb, result, glyphs);
  return result;
}

static mrb_value
mrb_s_maru12(mrb_state *mrb, mrb_value self)
{
  return array_of_shinonome(mrb, 12, ascii_12, jis208_12maru);
}

static mrb_value
mrb_s_go12(mrb_state *mrb, mrb_value self)
{
  return array_of_shinonome(mrb, 12, ascii_12, jis208_12go);
}

static mrb_value
mrb_s_min12(mrb_state *mrb, mrb_value self)
{
  return array_of_shinonome(mrb, 12, ascii_12, jis208_12min);
}

static mrb_value
mrb_s_go16(mrb_state *mrb, mrb_value self)
{
  return array_of_shinonome(mrb, 16, ascii_16, jis208_16go);
}

static mrb_value
mrb_s_min16(mrb_state *mrb, mrb_value self)
{
  return array_of_shinonome(mrb, 16, ascii_16, jis208_16min);
}


void
mrb_picoruby_shinonome_gem_init(mrb_state* mrb)
{
  struct RClass *module_Shinonome = mrb_define_module_id(mrb, MRB_SYM(Shinonome));

  mrb_define_class_method_id(mrb, module_Shinonome, MRB_SYM(test12), mrb_s_test12, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, module_Shinonome, MRB_SYM(test16), mrb_s_test16, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, module_Shinonome, MRB_SYM(maru12), mrb_s_maru12, MRB_ARGS_ARG(1,1));
  mrb_define_class_method_id(mrb, module_Shinonome, MRB_SYM(go12),   mrb_s_go12,   MRB_ARGS_ARG(1,1));
  mrb_define_class_method_id(mrb, module_Shinonome, MRB_SYM(min12),  mrb_s_min12,  MRB_ARGS_ARG(1,1));
  mrb_define_class_method_id(mrb, module_Shinonome, MRB_SYM(go16),   mrb_s_go16,   MRB_ARGS_ARG(1,1));
  mrb_define_class_method_id(mrb, module_Shinonome, MRB_SYM(min16),  mrb_s_min16,  MRB_ARGS_ARG(1,1));
}

void
mrb_picoruby_shinonome_gem_final(mrb_state* mrb)
{
}

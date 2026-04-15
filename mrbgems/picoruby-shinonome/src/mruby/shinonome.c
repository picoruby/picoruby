#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/string.h>
#include <mruby/array.h>
#include "bdffont_mruby.h"
#include "picoruby/debug.h"

static void
test_print(mrb_state *mrb, mrb_int size)
{
  const char *text;
  mrb_get_args(mrb, "z", &text);
  const char *p = text;
  while (*p) {
    uint32_t c = bdffont_utf8_to_unicode((const char **)&p);
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
      D("unicode: %04X\n", c);
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

static bool
maru12_glyph(const char **p, uint64_t *lines)
{
  return array_of_shinonome_sub(p, lines, 12, ascii_12, jis208_12maru);
}

static bool
go12_glyph(const char **p, uint64_t *lines)
{
  return array_of_shinonome_sub(p, lines, 12, ascii_12, jis208_12go);
}

static bool
min12_glyph(const char **p, uint64_t *lines)
{
  return array_of_shinonome_sub(p, lines, 12, ascii_12, jis208_12min);
}

static bool
go16_glyph(const char **p, uint64_t *lines)
{
  return array_of_shinonome_sub(p, lines, 16, ascii_16, jis208_16go);
}

static bool
min16_glyph(const char **p, uint64_t *lines)
{
  return array_of_shinonome_sub(p, lines, 16, ascii_16, jis208_16min);
}

static mrb_value
mrb_s_maru12(mrb_state *mrb, mrb_value self)
{
  const char *text;
  mrb_int scale = 1;
  mrb_get_args(mrb, "z|i", &text, &scale);
  return bdffont_render(mrb, text, scale, 12, maru12_glyph);
}

static mrb_value
mrb_s_go12(mrb_state *mrb, mrb_value self)
{
  const char *text;
  mrb_int scale = 1;
  mrb_get_args(mrb, "z|i", &text, &scale);
  return bdffont_render(mrb, text, scale, 12, go12_glyph);
}

static mrb_value
mrb_s_min12(mrb_state *mrb, mrb_value self)
{
  const char *text;
  mrb_int scale = 1;
  mrb_get_args(mrb, "z|i", &text, &scale);
  return bdffont_render(mrb, text, scale, 12, min12_glyph);
}

static mrb_value
mrb_s_go16(mrb_state *mrb, mrb_value self)
{
  const char *text;
  mrb_int scale = 1;
  mrb_get_args(mrb, "z|i", &text, &scale);
  return bdffont_render(mrb, text, scale, 16, go16_glyph);
}

static mrb_value
mrb_s_min16(mrb_state *mrb, mrb_value self)
{
  const char *text;
  mrb_int scale = 1;
  mrb_get_args(mrb, "z|i", &text, &scale);
  return bdffont_render(mrb, text, scale, 16, min16_glyph);
}

void
mrb_picoruby_shinonome_gem_init(mrb_state *mrb)
{
  struct RClass *module_Shinonome = mrb_define_module_id(mrb, MRB_SYM(Shinonome));

  mrb_define_class_method_id(mrb, module_Shinonome, MRB_SYM(test12), mrb_s_test12, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, module_Shinonome, MRB_SYM(test16), mrb_s_test16, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, module_Shinonome, MRB_SYM(maru12), mrb_s_maru12, MRB_ARGS_ARG(1, 1));
  mrb_define_class_method_id(mrb, module_Shinonome, MRB_SYM(go12),   mrb_s_go12,   MRB_ARGS_ARG(1, 1));
  mrb_define_class_method_id(mrb, module_Shinonome, MRB_SYM(min12),  mrb_s_min12,  MRB_ARGS_ARG(1, 1));
  mrb_define_class_method_id(mrb, module_Shinonome, MRB_SYM(go16),   mrb_s_go16,   MRB_ARGS_ARG(1, 1));
  mrb_define_class_method_id(mrb, module_Shinonome, MRB_SYM(min16),  mrb_s_min16,  MRB_ARGS_ARG(1, 1));
}

void
mrb_picoruby_shinonome_gem_final(mrb_state *mrb)
{
}

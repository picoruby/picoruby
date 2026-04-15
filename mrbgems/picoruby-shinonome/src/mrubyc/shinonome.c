#include <mrubyc.h>
#include "bdffont_mrubyc.h"
#include "picoruby/debug.h"

static void
test_print(mrbc_vm *vm, mrbc_value *v, mrbc_int_t size)
{
  const char *text = (const char *)GET_STRING_ARG(1);
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

static void
c_s_maru12(mrbc_vm *vm, mrbc_value *v, int argc)
{
  const char *text = (const char *)GET_STRING_ARG(1);
  mrbc_int_t scale = (argc > 1) ? GET_INT_ARG(2) : 1;
  mrbc_value result = bdffont_render(vm, text, scale, 12, maru12_glyph);
  SET_RETURN(result);
}

static void
c_s_go12(mrbc_vm *vm, mrbc_value *v, int argc)
{
  const char *text = (const char *)GET_STRING_ARG(1);
  mrbc_int_t scale = (argc > 1) ? GET_INT_ARG(2) : 1;
  mrbc_value result = bdffont_render(vm, text, scale, 12, go12_glyph);
  SET_RETURN(result);
}

static void
c_s_min12(mrbc_vm *vm, mrbc_value *v, int argc)
{
  const char *text = (const char *)GET_STRING_ARG(1);
  mrbc_int_t scale = (argc > 1) ? GET_INT_ARG(2) : 1;
  mrbc_value result = bdffont_render(vm, text, scale, 12, min12_glyph);
  SET_RETURN(result);
}

static void
c_s_go16(mrbc_vm *vm, mrbc_value *v, int argc)
{
  const char *text = (const char *)GET_STRING_ARG(1);
  mrbc_int_t scale = (argc > 1) ? GET_INT_ARG(2) : 1;
  mrbc_value result = bdffont_render(vm, text, scale, 16, go16_glyph);
  SET_RETURN(result);
}

static void
c_s_min16(mrbc_vm *vm, mrbc_value *v, int argc)
{
  const char *text = (const char *)GET_STRING_ARG(1);
  mrbc_int_t scale = (argc > 1) ? GET_INT_ARG(2) : 1;
  mrbc_value result = bdffont_render(vm, text, scale, 16, min16_glyph);
  SET_RETURN(result);
}

void
mrbc_shinonome_init(mrbc_vm *vm)
{
  mrbc_class *module_Shinonome = mrbc_define_module(vm, "Shinonome");

  mrbc_define_method(vm, module_Shinonome, "test12", c_s_test12);
  mrbc_define_method(vm, module_Shinonome, "test16", c_s_test16);
  mrbc_define_method(vm, module_Shinonome, "maru12", c_s_maru12);
  mrbc_define_method(vm, module_Shinonome, "go12",   c_s_go12);
  mrbc_define_method(vm, module_Shinonome, "min12",  c_s_min12);
  mrbc_define_method(vm, module_Shinonome, "go16",   c_s_go16);
  mrbc_define_method(vm, module_Shinonome, "min16",  c_s_min16);
}

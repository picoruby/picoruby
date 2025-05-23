#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/string.h>
#include <mruby/array.h>

#include <stdbool.h>
#include <string.h>

#include "unicode2jis.h"
#include "ascii_12_table.h"
#include "ascii_16_table.h"
#include "jis208_12maru_table.h"
#include "jis208_12go_table.h"
#include "jis208_12min_table.h"
#include "jis208_16go_table.h"
#include "jis208_16min_table.h"

static void
print_ascii_12(uint8_t asciicode)
{
  const uint8_t *data = &ascii_12[(asciicode - 0x20) * 9];
  int pos = 0;
  for (int i = 0; i < 9; i++) {
    uint8_t byte = data[i];
    for (int j = 7; -1 < j ; j--) { if ((byte>>j)&1) {
        printf("\xe2\x96\x88\xe2\x96\x88");
      } else {
        printf("__");
      }
      pos++;
      if (pos % 6 == 0) {
        printf("\n");
      }
    }
  }
}

static void
print_ascii_16(uint8_t asciicode)
{
  const uint8_t *data = &ascii_16[(asciicode - 0x20) * 16];
  int pos = 0;
  for (int i = 0; i < 16; i++) {
    uint8_t byte = data[i];
    for (int j = 7; -1 < j ; j--) { if ((byte>>j)&1) {
        printf("\xe2\x96\x88\xe2\x96\x88");
      } else {
        printf("__");
      }
      pos++;
      if (pos % 8 == 0) {
        printf("\n");
      }
    }
  }
}

static void
print_jis208_12(uint16_t jiscode)
{
  uint8_t first_byte = jiscode >> 8;
  uint8_t second_byte = jiscode & 0xFF;
  const uint8_t *table = jis208_12maru[first_byte - 0x21];
  const uint8_t *data = table + (second_byte - 0x21) * 18;
  int pos = 0;
  for (int i = 0; i < 18; i++) {
    uint8_t byte = data[i];
    for (int j = 7; -1 < j ; j--) {
      if ((byte>>j)&1) {
        printf("\xe2\x96\x88\xe2\x96\x88");
      } else {
        printf("__");
      }
      pos++;
      if (pos % 12 == 0) {
        printf("\n");
      }
    }
  }
}

static void
print_jis208_16(uint16_t jiscode)
{
  uint8_t first_byte = jiscode >> 8;
  uint8_t second_byte = jiscode & 0xFF;
  const uint8_t *table = jis208_16go[first_byte - 0x21];
  const uint8_t *data = table + (second_byte - 0x21) * 32;
  int pos = 0;
  for (int i = 0; i < 32; i++) {
    uint8_t byte = data[i];
    for (int j = 7; -1 < j ; j--) {
      if ((byte>>j)&1) {
        printf("\xe2\x96\x88\xe2\x96\x88");
      } else {
        printf("__");
      }
      pos++;
      if (pos % 16 == 0) {
        printf("\n");
      }
    }
  }
}

static inline uint32_t
utf8_to_unicode(const char **p)
{
  const uint8_t *s = (const uint8_t *)*p;
  uint32_t cp;

  if (s[0] < 0x80) {
    cp = s[0];
    *p += 1;
  } else if ((s[0] & 0xE0) == 0xC0) {
    cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
    *p += 2;
  } else if ((s[0] & 0xF0) == 0xE0) {
    cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    *p += 3;
  } else {
    // 非対応
    cp = 0xFFFD;
    *p += 1;
  }
  return cp;
}

static inline uint16_t
unicode_to_jis(uint16_t unicode)
{
  int left = 0, right = sizeof(unicode2jis) / sizeof(unicode2jis[0]) - 1;
  while (left <= right) {
    int mid = (left + right) / 2;
    if (unicode2jis[mid].unicode == unicode)
      return unicode2jis[mid].jis;
    else if (unicode2jis[mid].unicode < unicode)
      left = mid + 1;
    else
      right = mid - 1;
  }
  return 0;
}

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

static bool
array_of_shinonome_sub(const char **p, uint64_t *lines, mrb_int size, const uint8_t *ascii_table, const uint8_t **jis208_table)
{
  uint32_t c = utf8_to_unicode(p);
  if (c < 0x80) {
    lines[0] = size / 2;
    if (size == 12) { // size is 6 (half width)
      const uint8_t *data = &ascii_table[(c - 0x20) * 9];
      // 00000000 00000000 00000000 | repeat 3 times
      // ^^^^^^       ^^^^ ^^       |
      //       ^^ ^^^^       ^^^^^^ |
      for (int i = 0; i < 3; i++) {
        lines[i*4+1] = (data[i*3]>>2);
        lines[i*4+2] = ((data[i*3]&0x3)<<4)|(data[i*3+1]>>4);
        lines[i*4+3] = ((data[i*3+1]&0xF)<<2)|(data[i*3+2]>>6);
        lines[i*4+4] = (data[i*3+2]&0x3F);
      }
    } else if (size == 16) { // size is 8 (half width)
      const uint8_t *data = &ascii_table[(c - 0x20) * 16];
      for (int i = 0; i < 16; i++) {
        lines[i+1] = data[i];
      }
    }
  } else if (c < 0x10000) {
    lines[0] = size;
    uint16_t jiscode = unicode_to_jis(c);
    uint8_t first_byte = jiscode >> 8;
    uint8_t second_byte = jiscode & 0xFF;
    const uint8_t *table = jis208_table[first_byte - 0x21];
    if (size == 12) { // size is 12 (full width)
      // 00000000 00000000 00000000 | repeat 6 times
      // ^^^^^^^^ ^^^^              |
      //              ^^^^ ^^^^^^^^ |
      const uint8_t *data = table + (second_byte - 0x21) * 18;
      for (int i = 0; i < 6; i++) {
        lines[i*2+1] = (data[i*3]<<4)|(data[i*3+1]>>4);
        lines[i*2+2] = ((data[i*3+1]&0xF)<<8)|data[i*3+2];
      }
    } else if (size == 16) { // size is 16 (full width)
      const uint8_t *data = table + (second_byte - 0x21) * 32;
      for (int i = 0; i < 16; i++) {
        lines[i+1] = data[i*2]<<8|data[i*2+1];
      }
    }
  } else {
    return false;
  }
  return true;
}

static uint64_t
expand_bits(uint64_t src, int bit_count, int scale)
{
  uint64_t dst __attribute__((aligned(8))) = 0;
  for (int i = 0; i < bit_count; i++) {
    uint8_t bit = (src >> (bit_count - 1 - i)) & 1;
    dst <<= scale;

    if (bit) {
      dst |= (1 << scale) - 1;
    }
  }
  return dst;
}

#define GET_BIT(row, x, w) (((row) >> (w - 1 - (x))) & 1)
#define SET_BIT(row, x, w) ((row) |= ((uint64_t)1 << (w - 1 - (x))))

void
smooth_edges(uint64_t *input, int w, int h)
{
  uint64_t tmp[h] __attribute__((aligned(8)));
  memcpy(tmp, input, sizeof(uint64_t) * h);

  for (int y = 0; y < h - 1; y++) {
    for (int x = 0; x < w - 1; x++) {
      int a = GET_BIT(input[y], x, w);
      int b = GET_BIT(input[y], x + 1, w);
      int c = GET_BIT(input[y + 1], x, w);
      int d = GET_BIT(input[y + 1], x + 1, w);
      if ((a && d && !b && !c) || (b && c && !a && !d)) {
        SET_BIT(tmp[y + 1], x + 1, w);
      }
    }
  }
  for (int i = 0; i < h; i++) {
    input[i] = tmp[i];
  }
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
    if (!array_of_shinonome_sub(&p, lines, size, ascii_table, jis208_table)) {
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

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "bdffont.h"
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
  int i = 0;
  while (i < 9) {
    uint8_t byte = data[i];
    int j = 7;
    while (-1 < j) {
      if ((byte >> j) & 1) {
        printf("\xe2\x96\x88\xe2\x96\x88");
      } else {
        printf("__");
      }
      pos++;
      if (pos % 6 == 0) {
        printf("\n");
      }
      j--;
    }
    i++;
  }
}

static void
print_ascii_16(uint8_t asciicode)
{
  const uint8_t *data = &ascii_16[(asciicode - 0x20) * 16];
  int pos = 0;
  int i = 0;
  while (i < 16) {
    uint8_t byte = data[i];
    int j = 7;
    while (-1 < j) {
      if ((byte >> j) & 1) {
        printf("\xe2\x96\x88\xe2\x96\x88");
      } else {
        printf("__");
      }
      pos++;
      if (pos % 8 == 0) {
        printf("\n");
      }
      j--;
    }
    i++;
  }
}

static void
print_jis208_12(uint16_t jiscode)
{
  uint8_t first_byte  = jiscode >> 8;
  uint8_t second_byte = jiscode & 0xFF;
  const uint8_t *table = jis208_12maru[first_byte - 0x21];
  const uint8_t *data  = table + (second_byte - 0x21) * 18;
  int pos = 0;
  int i = 0;
  while (i < 18) {
    uint8_t byte = data[i];
    int j = 7;
    while (-1 < j) {
      if ((byte >> j) & 1) {
        printf("\xe2\x96\x88\xe2\x96\x88");
      } else {
        printf("__");
      }
      pos++;
      if (pos % 12 == 0) {
        printf("\n");
      }
      j--;
    }
    i++;
  }
}

static void
print_jis208_16(uint16_t jiscode)
{
  uint8_t first_byte  = jiscode >> 8;
  uint8_t second_byte = jiscode & 0xFF;
  const uint8_t *table = jis208_16go[first_byte - 0x21];
  const uint8_t *data  = table + (second_byte - 0x21) * 32;
  int pos = 0;
  int i = 0;
  while (i < 32) {
    uint8_t byte = data[i];
    int j = 7;
    while (-1 < j) {
      if ((byte >> j) & 1) {
        printf("\xe2\x96\x88\xe2\x96\x88");
      } else {
        printf("__");
      }
      pos++;
      if (pos % 16 == 0) {
        printf("\n");
      }
      j--;
    }
    i++;
  }
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

static bool
array_of_shinonome_sub(const char **p, uint64_t *lines, size_t size,
                       const uint8_t *ascii_table, const uint8_t **jis208_table)
{
  uint32_t c = bdffont_utf8_to_unicode(p);
  if (c < 0x80) {
    lines[0] = size / 2;
    if (size == 12) {
      const uint8_t *data = &ascii_table[(c - 0x20) * 9];
      int i = 0;
      while (i < 3) {
        lines[i*4+1] = (data[i*3]>>2);
        lines[i*4+2] = ((data[i*3]&0x3)<<4)|(data[i*3+1]>>4);
        lines[i*4+3] = ((data[i*3+1]&0xF)<<2)|(data[i*3+2]>>6);
        lines[i*4+4] = (data[i*3+2]&0x3F);
        i++;
      }
    } else if (size == 16) {
      const uint8_t *data = &ascii_table[(c - 0x20) * 16];
      int i = 0;
      while (i < 16) {
        lines[i+1] = data[i];
        i++;
      }
    }
  } else if (c < 0x10000) {
    lines[0] = size;
    uint16_t jiscode    = unicode_to_jis(c);
    uint8_t first_byte  = jiscode >> 8;
    uint8_t second_byte = jiscode & 0xFF;
    const uint8_t *table = jis208_table[first_byte - 0x21];
    if (size == 12) {
      const uint8_t *data = table + (second_byte - 0x21) * 18;
      int i = 0;
      while (i < 6) {
        lines[i*2+1] = (data[i*3]<<4)|(data[i*3+1]>>4);
        lines[i*2+2] = ((data[i*3+1]&0xF)<<8)|data[i*3+2];
        i++;
      }
    } else if (size == 16) {
      const uint8_t *data = table + (second_byte - 0x21) * 32;
      int i = 0;
      while (i < 16) {
        lines[i+1] = data[i*2]<<8|data[i*2+1];
        i++;
      }
    }
  } else {
    return false;
  }
  return true;
}


#if defined(PICORB_VM_MRUBY)

#include "mruby/shinonome.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/shinonome.c"

#endif

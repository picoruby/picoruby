#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "unicode2jis.c"
#include "ascii_12_table.c"
#include "ascii_16_table.c"
#include "jis208_12maru_table.c"
#include "jis208_12go_table.c"
#include "jis208_12min_table.c"
#include "jis208_16go_table.c"
#include "jis208_16min_table.c"

void
print_ascii_12(uint8_t asciicode)
{
  uint8_t *data = &ascii_12[(asciicode - 0x20) * 9];
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

void
print_ascii_16(uint8_t asciicode)
{
  uint8_t *data = &ascii_16[(asciicode - 0x20) * 16];
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

void
print_jis208_12(uint16_t jiscode)
{
  uint8_t first_byte = jiscode >> 8;
  uint8_t second_byte = jiscode & 0xFF;
  uint8_t *table = jis208_12maru[first_byte - 0x21];
  uint8_t *data = table + (second_byte - 0x21) * 18;
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

void
print_jis208_16(uint16_t jiscode)
{
  uint8_t first_byte = jiscode >> 8;
  uint8_t second_byte = jiscode & 0xFF;
  uint8_t *table = jis208_16go[first_byte - 0x21];
  uint8_t *data = table + (second_byte - 0x21) * 32;
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

uint32_t
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

uint16_t
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

int
main(void)
{
  const char *text = "Hello羽角です苑";
  const char *p = text;
  while (*p) {
    uint32_t c = utf8_to_unicode((const char **)&p);
    if (c < 0x80) {
      //print_ascii_12((uint8_t)c);
      print_ascii_16((uint8_t)c);
    } else if (c < 0x10000) {
      uint16_t jis = unicode_to_jis(c);
      if (jis) {
        //print_jis208_12(jis);
        print_jis208_16(jis);
      }
    } else {
      printf("unicode: %04X\n", c);
    }
  }
  return 0;
}

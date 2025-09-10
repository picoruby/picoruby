#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "terminus_6x12_table.h"
#include "terminus_8x16_table.h"
#include "terminus_12x24_table.h"
#include "terminus_16x32_table.h"

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
    // unsupported
    cp = 0xFFFD;
    *p += 1;
  }
  return cp;
}

static bool
array_of_terminus_sub(const char **p, uint64_t *lines, int w, int h, const uint8_t *ascii_table)
{
  uint32_t c = utf8_to_unicode(p);

  // Only ASCII supported
  if (c < 0x20 || c > 0x7E) {
    return false;
  }

  lines[0] = w;
  int bytes_per_char = ((w * h) + 7) / 8;
  const uint8_t *data = &ascii_table[(c - 0x20) * bytes_per_char];

  // Convert byte data to lines
  int byte_idx = 0;
  int bit_idx = 0;

  for (int y = 1; y <= h; y++) {
    uint64_t line = 0;
    for (int x = 0; x < w; x++) {
      if (byte_idx < bytes_per_char) {
        int bit = (data[byte_idx] >> (7 - bit_idx)) & 1;
        if (bit) {
          line |= ((uint64_t)1 << (w - 1 - x));
        }
        bit_idx++;
        if (bit_idx >= 8) {
          bit_idx = 0;
          byte_idx++;
        }
      }
    }
    lines[y] = line;
  }

  return true;
}

static bool
array_of_terminus_6x12(const char **p, uint64_t *lines)
{
  return array_of_terminus_sub(p, lines, 6, 12, terminus_6x12);
}

static bool
array_of_terminus_8x16(const char **p, uint64_t *lines)
{
  return array_of_terminus_sub(p, lines, 8, 16, terminus_8x16);
}

static bool
array_of_terminus_12x24(const char **p, uint64_t *lines)
{
  return array_of_terminus_sub(p, lines, 12, 24, terminus_12x24);
}

static bool
array_of_terminus_16x32(const char **p, uint64_t *lines)
{
  return array_of_terminus_sub(p, lines, 16, 32, terminus_16x32);
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/terminus.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/terminus.c"

#endif

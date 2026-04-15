#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "bdffont.h"

uint64_t
bdffont_expand_bits(uint64_t src, int bit_count, int scale)
{
  uint64_t dst __attribute__((aligned(8))) = 0;
  int i = 0;
  while (i < bit_count) {
    uint8_t bit = (src >> (bit_count - 1 - i)) & 1;
    dst <<= scale;
    if (bit) {
      dst |= (1 << scale) - 1;
    }
    i++;
  }
  return dst;
}

#define GET_BIT(row, x, w) (((row) >> ((w) - 1 - (x))) & 1)
#define SET_BIT(row, x, w) ((row) |= ((uint64_t)1 << ((w) - 1 - (x))))

void
bdffont_smooth_edges(uint64_t *input, int w, int h)
{
  uint64_t tmp[h] __attribute__((aligned(8)));
  memcpy(tmp, input, sizeof(uint64_t) * h);

  int y = 0;
  while (y < h - 1) {
    int x = 0;
    while (x < w - 1) {
      int a = GET_BIT(input[y],     x,     w);
      int b = GET_BIT(input[y],     x + 1, w);
      int c = GET_BIT(input[y + 1], x,     w);
      int d = GET_BIT(input[y + 1], x + 1, w);

      if (b && c && !a && !d) {
        SET_BIT(tmp[y],     x,     w);
        SET_BIT(tmp[y + 1], x + 1, w);
      }
      if (a && d && !b && !c) {
        SET_BIT(tmp[y],     x + 1, w);
        SET_BIT(tmp[y + 1], x,     w);
      }
      x++;
    }
    y++;
  }
  int i = 0;
  while (i < h) {
    input[i] = tmp[i];
    i++;
  }
}

uint32_t
bdffont_utf8_to_unicode(const char **p)
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
    /* unsupported multi-byte sequence */
    cp = 0xFFFD;
    *p += 1;
  }
  return cp;
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/bdffont_render.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/bdffont_render.c"

#endif

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "karmatic_arcade_20_table.h"

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

static void
smooth_edges(uint64_t *input, int w, int h)
{
  uint64_t tmp[h] __attribute__((aligned(8)));
  memcpy(tmp, input, sizeof(uint64_t) * h);

  for (int y = 0; y < h - 1; y++) {
    for (int x = 0; x < w - 1; x++) {
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
    }
  }
  for (int i = 0; i < h; i++) {
    input[i] = tmp[i];
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
    // unsupported
    cp = 0xFFFD;
    *p += 1;
  }
  return cp;
}

/* Extract one character's bitmap.
 * lines[0]         = character cell width (pixels)
 * lines[1..HEIGHT] = bitmap rows, pixel 0 at MSB (bit cell_w-1)
 */
static bool
array_of_karmatic_arcade_sub(const char **p, uint64_t *lines)
{
  uint32_t c = utf8_to_unicode(p);

  if (c < KARMATIC_ARCADE_ASCII_BASE || c > 0x7E) {
    return false;
  }

  int idx      = c - KARMATIC_ARCADE_ASCII_BASE;
  int cell_w   = karmatic_arcade_widths[idx];
  const uint8_t *data = &karmatic_arcade_bitmaps[karmatic_arcade_offsets[idx]];

  lines[0] = cell_w;

  int byte_idx = 0;
  int bit_idx  = 0;

  for (int y = 0; y < KARMATIC_ARCADE_HEIGHT; y++) {
    uint64_t line = 0;
    for (int x = 0; x < cell_w; x++) {
      int bit = (data[byte_idx] >> (7 - bit_idx)) & 1;
      if (bit) {
        line |= ((uint64_t)1 << (cell_w - 1 - x));
      }
      bit_idx++;
      if (bit_idx >= 8) {
        bit_idx = 0;
        byte_idx++;
      }
    }
    lines[y + 1] = line;
  }

  return true;
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/karmatic_arcade.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/karmatic_arcade.c"

#endif

#include <stdbool.h>
#include <string.h>

#include "bdffont.h"
#include "karmatic_arcade_10_table.h"

/* Extract one character's bitmap.
 * lines[0]         = character cell width (pixels)
 * lines[1..HEIGHT] = bitmap rows, pixel 0 at MSB (bit cell_w-1)
 */
static bool
array_of_karmatic_arcade_sub(const char **p, uint64_t *lines)
{
  uint32_t c = bdffont_utf8_to_unicode(p);

  if (c < KARMATIC_ARCADE_ASCII_BASE || 0x7E < c) {
    return false;
  }

  int idx    = c - KARMATIC_ARCADE_ASCII_BASE;
  int cell_w = karmatic_arcade_widths[idx];
  const uint8_t *data = &karmatic_arcade_bitmaps[karmatic_arcade_offsets[idx]];

  lines[0] = cell_w;

  int byte_idx = 0;
  int bit_idx  = 0;

  int y = 0;
  while (y < KARMATIC_ARCADE_HEIGHT) {
    uint64_t line = 0;
    int x = 0;
    while (x < cell_w) {
      int bit = (data[byte_idx] >> (7 - bit_idx)) & 1;
      if (bit) {
        line |= ((uint64_t)1 << (cell_w - 1 - x));
      }
      bit_idx++;
      if (bit_idx >= 8) {
        bit_idx = 0;
        byte_idx++;
      }
      x++;
    }
    lines[y + 1] = line;
    y++;
  }

  return true;
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/karmatic_arcade.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/karmatic_arcade.c"

#endif

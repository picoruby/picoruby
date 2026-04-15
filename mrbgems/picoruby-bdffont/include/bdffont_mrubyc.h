#include <mrubyc.h>
#include "bdffont.h"

/* Render text using the given glyph callback.
 * Returns [height, total_width, widths, glyphs].
 * Returns nil on invalid scale or unsupported character (mrubyc does not raise). */
mrbc_value bdffont_render(mrbc_vm *vm, const char *text,
                          mrbc_int_t scale, int cell_h, bdffont_glyph_fn fn);

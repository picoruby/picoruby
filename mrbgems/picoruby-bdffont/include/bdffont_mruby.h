#include <mruby.h>
#include <mruby/array.h>
#include "bdffont.h"

/* Render text using the given glyph callback.
 * Returns [height, total_width, widths, glyphs].
 * Raises ArgumentError on invalid scale or unsupported character. */
mrb_value bdffont_render(mrb_state *mrb, const char *text,
                         mrb_int scale, int cell_h, bdffont_glyph_fn fn);

#ifndef BDFFONT_H_
#define BDFFONT_H_

#include <stdint.h>
#include <stdbool.h>

/* Callback: extract one glyph from UTF-8 stream.
 * lines[0]    = cell width (pixels, unscaled)
 * lines[1..h] = bitmap rows (MSB = leftmost pixel)
 * Returns false for unsupported characters. */
typedef bool (*bdffont_glyph_fn)(const char **p, uint64_t *lines);

uint64_t bdffont_expand_bits(uint64_t src, int bit_count, int scale);
void     bdffont_smooth_edges(uint64_t *input, int w, int h);
uint32_t bdffont_utf8_to_unicode(const char **p);

#endif /* BDFFONT_H_ */

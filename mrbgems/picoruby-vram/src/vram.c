#include "../include/vram.h"
#include <string.h>

/*
 * Buffer size calculation.
 * When rotate=90 or 270, the buffer stores the transposed image (buf_w=h, buf_h=w).
 *   vertical   layout: buf_w * ceil(buf_h / 8)
 *   horizontal layout: ceil(buf_w / 8) * buf_h
 */
static size_t
display_page_buffer_size(int page_w, int page_h, bool horizontal, int rotate)
{
  int bw = (rotate == 90 || rotate == 270) ? page_h : page_w;
  int bh = (rotate == 90 || rotate == 270) ? page_w : page_h;
  if (horizontal) {
    return (size_t)(((bw + 7) / 8) * bh);
  } else {
    return (size_t)(bw * ((bh + 7) / 8));
  }
}

/*
 * Vertical layout (SSD1306 style): 8 vertical pixels per byte, LSB = topmost pixel.
 *   byte_idx = x + (y / 8) * page_w
 *   bit_idx  = y % 8
 */
static void
mono_page_set_pixel(struct display_page *page, int x, int y, uint32_t color)
{
  if (x < 0 || x >= page->buf_w || y < 0 || y >= page->buf_h) return;
  if (page->invert) color = color ? 0 : 1;
  int byte_idx = x + (y / 8) * page->buf_w;
  int bit_idx = y % 8;
  if (byte_idx < 0 || (size_t)byte_idx >= page->raw_size) return;
  if (color) {
    page->raw_data[byte_idx] |= (uint8_t)(1 << bit_idx);
  } else {
    page->raw_data[byte_idx] &= (uint8_t)~(1 << bit_idx);
  }
  page->dirty = true;
}

static uint32_t
mono_page_get_pixel(struct display_page *page, int x, int y)
{
  if (x < 0 || x >= page->buf_w || y < 0 || y >= page->buf_h) return 0;
  int byte_idx = x + (y / 8) * page->buf_w;
  int bit_idx = y % 8;
  if (byte_idx < 0 || (size_t)byte_idx >= page->raw_size) return 0;
  uint32_t pixel = (page->raw_data[byte_idx] >> bit_idx) & 1;
  if (page->invert) pixel = pixel ? 0 : 1;
  return pixel;
}

static void
mono_page_fill(struct display_page *page, uint32_t color)
{
  if (page->invert) color = color ? 0 : 1;
  memset(page->raw_data, color ? 0xFF : 0x00, page->raw_size);
  page->dirty = true;
}

/*
 * Horizontal layout (UC8151 style): 8 horizontal pixels per byte, MSB = leftmost pixel.
 *   byte_idx = (x / 8) + y * ((page_w + 7) / 8)
 *   bit_idx  = 7 - (x % 8)
 */
static void
mono_h_page_set_pixel(struct display_page *page, int x, int y, uint32_t color)
{
  if (x < 0 || x >= page->buf_w || y < 0 || y >= page->buf_h) return;
  if (page->invert) color = color ? 0 : 1;
  int byte_idx = (x / 8) + y * ((page->buf_w + 7) / 8);
  int bit_idx = 7 - (x % 8);
  if (byte_idx < 0 || (size_t)byte_idx >= page->raw_size) return;
  if (color) {
    page->raw_data[byte_idx] |= (uint8_t)(1 << bit_idx);
  } else {
    page->raw_data[byte_idx] &= (uint8_t)~(1 << bit_idx);
  }
  page->dirty = true;
}

static uint32_t
mono_h_page_get_pixel(struct display_page *page, int x, int y)
{
  if (x < 0 || x >= page->buf_w || y < 0 || y >= page->buf_h) return 0;
  int byte_idx = (x / 8) + y * ((page->buf_w + 7) / 8);
  int bit_idx = 7 - (x % 8);
  if (byte_idx < 0 || (size_t)byte_idx >= page->raw_size) return 0;
  uint32_t pixel = (page->raw_data[byte_idx] >> bit_idx) & 1;
  if (page->invert) pixel = pixel ? 0 : 1;
  return pixel;
}

static void
mono_h_page_fill(struct display_page *page, uint32_t color)
{
  if (page->invert) color = color ? 0 : 1;
  memset(page->raw_data, color ? 0xFF : 0x00, page->raw_size);
  page->dirty = true;
}

/*
 * Select pixel format function pointers and compute buffer dimensions.
 * Must be called after page->w, page->h, and page->rotate are set.
 */
static void
display_page_init_format(display_page_t *page, bool horizontal)
{
  if (page->rotate == 90 || page->rotate == 270) {
    page->buf_w = page->h;
    page->buf_h = page->w;
  } else {
    page->buf_w = page->w;
    page->buf_h = page->h;
  }
  if (horizontal) {
    page->pixel_format = PIXEL_FORMAT_MONO_H;
    page->set_pixel = mono_h_page_set_pixel;
    page->get_pixel = mono_h_page_get_pixel;
    page->fill = mono_h_page_fill;
  } else {
    page->pixel_format = PIXEL_FORMAT_MONO;
    page->set_pixel = mono_page_set_pixel;
    page->get_pixel = mono_page_get_pixel;
    page->fill = mono_page_fill;
  }
}

/* Display-level helpers: route (x,y) to the correct page */

static void
display_set_pixel(display_t *disp, int x, int y, uint32_t color)
{
  int i = 0;
  while (i < disp->page_count) {
    display_page_t *page = &disp->pages[i];
    if (x >= page->x && x < page->x + page->w &&
        y >= page->y && y < page->y + page->h) {
      int lx = x - page->x;
      int ly = y - page->y;
      int bx, by;
      switch (page->rotate) {
        case 90:
          bx = ly;
          by = page->w - 1 - lx;
          break;
        case 180:
          bx = page->w - 1 - lx;
          by = page->h - 1 - ly;
          break;
        case 270:
          bx = page->h - 1 - ly;
          by = lx;
          break;
        default:
          bx = lx;
          by = ly;
          break;
      }
      page->set_pixel(page, bx, by, color);
      break;
    }
    i++;
  }
}

static void
display_draw_line(display_t *disp, int x0, int y0, int x1, int y1, uint32_t color)
{
  int dx = x1 > x0 ? x1 - x0 : x0 - x1;
  int dy = y1 > y0 ? y1 - y0 : y0 - y1;
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx - dy;
  int x = x0;
  int y = y0;
  while (1) {
    display_set_pixel(disp, x, y, color);
    if (x == x1 && y == y1) break;
    int e2 = 2 * err;
    if (e2 > -dy) { err -= dy; x += sx; }
    if (e2 < dx)  { err += dx; y += sy; }
  }
}

static void
display_draw_rect(display_t *disp, int x, int y, int w, int h, uint32_t color)
{
  int i = 0;
  while (i < h) {
    int j = 0;
    while (j < w) {
      display_set_pixel(disp, x + j, y + i, color);
      j++;
    }
    i++;
  }
}

static void
display_fill(display_t *disp, uint32_t color)
{
  int i = 0;
  while (i < disp->page_count) {
    disp->pages[i].fill(&disp->pages[i], color);
    i++;
  }
}

static void
display_erase(display_t *disp, int x, int y, int w, int h)
{
  int i = 0;
  while (i < h) {
    int j = 0;
    while (j < w) {
      display_set_pixel(disp, x + j, y + i, 0);
      j++;
    }
    i++;
  }
}

static void
display_draw_bytes(display_t *disp, int x, int y, int w, int h,
                   const uint8_t *data, size_t data_size)
{
  int img_y = 0;
  while (img_y < h) {
    int img_x = 0;
    while (img_x < w) {
      int byte_idx = (img_y * ((w + 7) / 8)) + (img_x / 8);
      int bit_idx = 7 - (img_x % 8);
      uint8_t pixel = 0;
      if ((size_t)byte_idx < data_size) {
        pixel = (data[byte_idx] >> bit_idx) & 1;
      }
      display_set_pixel(disp, x + img_x, y + img_y, pixel);
      img_x++;
    }
    img_y++;
  }
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/vram.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/vram.c"

#endif

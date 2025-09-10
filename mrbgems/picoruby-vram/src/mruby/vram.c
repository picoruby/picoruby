#include "mruby.h"
#include "mruby/class.h"
#include "mruby/array.h"
#include "mruby/string.h"
#include "mruby/presym.h"
#include "mruby/data.h"
#include "mruby/variable.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

static void
mrb_vram_free(mrb_state *mrb, void *ptr)
{
  display_t *disp = (display_t *)ptr;
  for (mrb_int i = 0; i < disp->page_count; i++) {
    display_page_t *page = &disp->pages[i];
    mrb_gc_unregister(mrb, page->buffer);
  }
  mrb_free(mrb, disp->pages);
  mrb_free(mrb, disp);
}

struct mrb_data_type mrb_vram_type = {
  "VRAM", mrb_vram_free,
};

static void
mono_page_set_pixel(struct display_page *page, int x, int y, uint32_t color)
{
  if (x < 0 || x >= page->w || y < 0 || y >= page->h) return;

  unsigned char *data = (unsigned char *)RSTRING_PTR(page->buffer);
  int byte_idx = x + (y / 8) * page->w;
  int bit_idx = y % 8;

  if (byte_idx < 0 || byte_idx >= RSTRING_LEN(page->buffer)) return;

  if (color) {
    data[byte_idx] |= (1 << bit_idx);
  } else {
    data[byte_idx] &= ~(1 << bit_idx);
  }
  page->dirty = true;
}

static uint32_t
mono_page_get_pixel(struct display_page *page, int x, int y)
{
  if (x < 0 || x >= page->w || y < 0 || y >= page->h) return 0;

  unsigned char *data = (unsigned char *)RSTRING_PTR(page->buffer);
  int byte_idx = x + (y / 8) * page->w;
  int bit_idx = y % 8;

  if (byte_idx < 0 || byte_idx >= RSTRING_LEN(page->buffer)) return 0;

  return (data[byte_idx] >> bit_idx) & 1;
}

static void
mono_page_fill(struct display_page *page, uint32_t color)
{
  unsigned char *data = (unsigned char *)RSTRING_PTR(page->buffer);
  memset(data, color ? 0xFF : 0x00, RSTRING_LEN(page->buffer));
  page->dirty = true;
}

/*
 * vram = VRAM.new(w: 128, h: 64, cols: 1, rows: 8)
 * vram.name = "SSD1306"
*/
static mrb_value
mrb_vram_s_new(mrb_state* mrb, mrb_value klass)
{
  const mrb_sym kw_names[] = { MRB_SYM(w), MRB_SYM(h), MRB_SYM(cols), MRB_SYM(rows) };
  mrb_value kw_values[4];
  mrb_value kw_rest;
  mrb_kwargs kwargs = { 4, 4, kw_names, kw_values, &kw_rest };
  mrb_get_args(mrb, ":", &kwargs);

  mrb_int w = mrb_fixnum(kw_values[0]);
  mrb_int h = mrb_fixnum(kw_values[1]);
  mrb_int cols = mrb_fixnum(kw_values[2]);
  mrb_int rows = mrb_fixnum(kw_values[3]);
  mrb_int page_w = w / cols;
  mrb_int page_h = h / rows;

  display_t *disp = mrb_malloc(mrb, sizeof(display_t));
  disp->w = w;
  disp->h = h;
  disp->page_count = rows * cols;
  disp->pages = mrb_malloc(mrb, sizeof(display_page_t) * disp->page_count);

  mrb_value vram = mrb_obj_value(Data_Wrap_Struct(mrb, mrb_class_ptr(klass), &mrb_vram_type, disp));

  mrb_int page_size = page_w * ((page_h + 7) / 8); // TODO: support formats

  for (mrb_int ty = 0; ty < rows; ty++) {
    for (mrb_int tx = 0; tx < cols; tx++) {
      display_page_t *page = &disp->pages[ty * cols + tx];
      page->x = tx * page_w;
      page->y = ty * page_h;
      page->w = page_w;
      page->h = page_h;
      // Create buffer with exact size needed
      page->buffer = mrb_str_new(mrb, NULL, page_size);
      char *data = RSTRING_PTR(page->buffer);
      memset(data, 0, page_size);
      mrb_gc_register(mrb, page->buffer);
      page->dirty = false;
      { // TODO: support formats
        page->pixel_format = PIXEL_FORMAT_MONO;
        page->set_pixel = mono_page_set_pixel;
        page->get_pixel = mono_page_get_pixel;
        page->fill = mono_page_fill;
      }
      page->fill(page, 0);
    }
  }

  return vram;
}

/*
 * vram.pages       => [[0, 0, "\x00..."], ] # All [col, row, data]
 * vram.dirty_pages => [[1, 2, "\x00..."], ] # [col, row, data] that are dirty
 */
static mrb_value
mrb_vram_pages_sub(mrb_state* mrb, mrb_value self, mrb_bool dirty)
{
  mrb_bool clear_dirty = true;
  mrb_get_args(mrb, "|b", &clear_dirty);
  mrb_value result = mrb_ary_new(mrb);
  display_t *disp = (display_t *)mrb_data_get_ptr(mrb, self, &mrb_vram_type);
  for (mrb_int i = 0; i < disp->page_count; i++) {
    display_page_t *page = &disp->pages[i];
    if (!dirty || page->dirty) {
      mrb_int cols = disp->w / page->w;
      mrb_int col = i % cols;
      mrb_int row = i / cols;
      mrb_value entry = mrb_ary_new_capa(mrb, 3);
      mrb_ary_push(mrb, entry, mrb_fixnum_value(col));
      mrb_ary_push(mrb, entry, mrb_fixnum_value(row));
      mrb_ary_push(mrb, entry, page->buffer);
      mrb_ary_push(mrb, result, entry);
      if (clear_dirty) page->dirty = false;
    }
  }
  return result;
}

static mrb_value
mrb_vram_pages(mrb_state* mrb, mrb_value self)
{
  return mrb_vram_pages_sub(mrb, self, false);
}

static mrb_value
mrb_vram_dirty_pages(mrb_state* mrb, mrb_value self)
{
  return mrb_vram_pages_sub(mrb, self, true);
}

static mrb_value
mrb_vram_draw_line(mrb_state* mrb, mrb_value self)
{
  mrb_int x0, y0, x1, y1, color;
  mrb_get_args(mrb, "iiiii", &x0, &y0, &x1, &y1, &color);

  display_t *disp = (display_t *)mrb_data_get_ptr(mrb, self, &mrb_vram_type);

  // Bresenham's line algorithm
  mrb_int dx = x1 > x0 ? x1 - x0 : x0 - x1;
  mrb_int dy = y1 > y0 ? y1 - y0 : y0 - y1;
  mrb_int sx = x0 < x1 ? 1 : -1;
  mrb_int sy = y0 < y1 ? 1 : -1;
  mrb_int err = dx - dy;

  mrb_int x = x0;
  mrb_int y = y0;

  while (1) {
    // Set pixel at current position
    for (mrb_int i = 0; i < disp->page_count; i++) {
      display_page_t *page = &disp->pages[i];
      if (x >= page->x && x < page->x + page->w &&
          y >= page->y && y < page->y + page->h) {
        page->set_pixel(page, x - page->x, y - page->y, color);
        break;
      }
    }

    if (x == x1 && y == y1) break;

    mrb_int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x += sx;
    }
    if (e2 < dx) {
      err += dx;
      y += sy;
    }
  }

  return self;
}

static mrb_value
mrb_vram_draw_rect(mrb_state* mrb, mrb_value self)
{
  mrb_int x, y, w, h, color;
  mrb_get_args(mrb, "iiiii", &x, &y, &w, &h, &color);

  display_t *disp = (display_t *)mrb_data_get_ptr(mrb, self, &mrb_vram_type);

  for (mrb_int i = 0; i < h; i++) {
    for (mrb_int j = 0; j < w; j++) {
      mrb_int px = x + j;
      mrb_int py = y + i;
      for (mrb_int k = 0; k < disp->page_count; k++) {
        display_page_t *page = &disp->pages[k];
        if (px >= page->x && px < page->x + page->w &&
            py >= page->y && py < page->y + page->h) {
          page->set_pixel(page, px - page->x, py - page->y, color);
          break;
        }
      }
    }
  }

  return self;
}

static mrb_value
mrb_vram_set_pixel(mrb_state* mrb, mrb_value self)
{
  mrb_int x, y, color;
  mrb_get_args(mrb, "iii", &x, &y, &color);

  display_t *disp = (display_t *)mrb_data_get_ptr(mrb, self, &mrb_vram_type);

  for (mrb_int i = 0; i < disp->page_count; i++) {
    display_page_t *page = &disp->pages[i];
    if (x >= page->x && x < page->x + page->w &&
        y >= page->y && y < page->y + page->h) {
      page->set_pixel(page, x - page->x, y - page->y, color);
      break;
    }
  }

  return self;
}

static mrb_value
mrb_vram_fill(mrb_state* mrb, mrb_value self)
{
  mrb_int color;
  mrb_get_args(mrb, "i", &color);
  display_t *disp = (display_t *)mrb_data_get_ptr(mrb, self, &mrb_vram_type);
  for (mrb_int i = 0; i < disp->page_count; i++) {
    display_page_t *page = &disp->pages[i];
    page->fill(page, color);
  }
  return self;
}

static mrb_value
mrb_vram_draw_bitmap(mrb_state* mrb, mrb_value self)
{
  const mrb_sym kw_names[] = { MRB_SYM(x), MRB_SYM(y), MRB_SYM(w), MRB_SYM(h), MRB_SYM(data) };
  mrb_value kw_values[5];
  mrb_value kw_rest;
  mrb_kwargs kwargs = { 5, 5, kw_names, kw_values, &kw_rest };
  mrb_get_args(mrb, ":", &kwargs);

  display_t *disp = (display_t *)mrb_data_get_ptr(mrb, self, &mrb_vram_type);
  if (!disp) return self;

  if (!mrb_integer_p(kw_values[0]) || !mrb_integer_p(kw_values[1]) ||
      !mrb_integer_p(kw_values[2]) || !mrb_integer_p(kw_values[3]) ||
      !mrb_array_p(kw_values[4])) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Invalid arguments for draw_bitmap");
  }

  mrb_int x = mrb_integer(kw_values[0]);
  mrb_int y = mrb_integer(kw_values[1]);
  mrb_int width = mrb_integer(kw_values[2]);
  mrb_int height = mrb_integer(kw_values[3]);
  mrb_value data_val = kw_values[4];

  if (width <= 0 || height <= 0) return self;

  mrb_int data_len = RARRAY_LEN(data_val);

  for (mrb_int img_y = 0; img_y < height && img_y < data_len; img_y++) {
    mrb_value row_val = mrb_ary_ref(mrb, data_val, img_y);
    if (mrb_integer_p(row_val)) {
      uint64_t row_data = (uint64_t)mrb_integer(row_val);
      for (mrb_int img_x = 0; img_x < width; img_x++) {
        uint8_t pixel = (row_data >> (width - 1 - img_x)) & 1;
        mrb_int screen_x = x + img_x;
        mrb_int screen_y = y + img_y;

        for (mrb_int i = 0; i < disp->page_count; i++) {
          display_page_t *page = &disp->pages[i];
          if (screen_x >= page->x && screen_x < page->x + page->w &&
              screen_y >= page->y && screen_y < page->y + page->h) {
            page->set_pixel(page, screen_x - page->x, screen_y - page->y, pixel);
            break;
          }
        }
      }
    }
  }

  return self;
}

static mrb_value
mrb_vram_draw_bytes(mrb_state* mrb, mrb_value self)
{
  const mrb_sym kw_names[] = { MRB_SYM(x), MRB_SYM(y), MRB_SYM(w), MRB_SYM(h), MRB_SYM(data) };
  mrb_value kw_values[5];
  mrb_value kw_rest;
  mrb_kwargs kwargs = { 5, 5, kw_names, kw_values, &kw_rest };
  mrb_get_args(mrb, ":", &kwargs);

  display_t *disp = (display_t *)mrb_data_get_ptr(mrb, self, &mrb_vram_type);
  if (!disp) return self;

  if (!mrb_integer_p(kw_values[0]) || !mrb_integer_p(kw_values[1]) ||
      !mrb_integer_p(kw_values[2]) || !mrb_integer_p(kw_values[3]) ||
      !mrb_string_p(kw_values[4])) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Invalid arguments for draw_bytes");
  }

  mrb_int x = mrb_integer(kw_values[0]);
  mrb_int y = mrb_integer(kw_values[1]);
  mrb_int width = mrb_integer(kw_values[2]);
  mrb_int height = mrb_integer(kw_values[3]);
  mrb_value data_val = kw_values[4];

  if (width <= 0 || height <= 0) return self;

  const uint8_t *image_data = (const uint8_t *)RSTRING_PTR(data_val);
  mrb_int data_size = RSTRING_LEN(data_val);

  for (mrb_int img_y = 0; img_y < height; img_y++) {
    for (mrb_int img_x = 0; img_x < width; img_x++) {
      mrb_int byte_idx = (img_y * ((width + 7) / 8)) + (img_x / 8);
      mrb_int bit_idx = 7 - (img_x % 8);

      uint8_t pixel = 0;
      if (byte_idx < data_size) {
        pixel = (image_data[byte_idx] >> bit_idx) & 1;
      }

      mrb_int screen_x = x + img_x;
      mrb_int screen_y = y + img_y;

      for (mrb_int i = 0; i < disp->page_count; i++) {
        display_page_t *page = &disp->pages[i];
        if (screen_x >= page->x && screen_x < page->x + page->w &&
            screen_y >= page->y && screen_y < page->y + page->h) {
          page->set_pixel(page, screen_x - page->x, screen_y - page->y, pixel);
          break;
        }
      }
    }
  }

  return self;
}


void
mrb_picoruby_vram_gem_init(mrb_state* mrb)
{
  struct RClass *class_VRAM = mrb_define_class_id(mrb, MRB_SYM(VRAM), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_VRAM, MRB_TT_CDATA);

  mrb_define_class_method_id(mrb, class_VRAM, MRB_SYM(new), mrb_vram_s_new, MRB_ARGS_KEY(4, 4));
  mrb_define_method_id(mrb, class_VRAM, MRB_SYM(pages), mrb_vram_pages, MRB_ARGS_OPT(1));
  mrb_define_method_id(mrb, class_VRAM, MRB_SYM(dirty_pages), mrb_vram_dirty_pages, MRB_ARGS_OPT(1));
  mrb_define_method_id(mrb, class_VRAM, MRB_SYM(draw_line), mrb_vram_draw_line, MRB_ARGS_REQ(5));
  mrb_define_method_id(mrb, class_VRAM, MRB_SYM(draw_rect), mrb_vram_draw_rect, MRB_ARGS_REQ(5));
  mrb_define_method_id(mrb, class_VRAM, MRB_SYM(draw_bitmap), mrb_vram_draw_bitmap, MRB_ARGS_KEY(5, 5));
  mrb_define_method_id(mrb, class_VRAM, MRB_SYM(draw_bytes), mrb_vram_draw_bytes, MRB_ARGS_KEY(5, 5));
  mrb_define_method_id(mrb, class_VRAM, MRB_SYM(set_pixel), mrb_vram_set_pixel, MRB_ARGS_REQ(3));
  mrb_define_method_id(mrb, class_VRAM, MRB_SYM(fill), mrb_vram_fill, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_vram_gem_final(mrb_state* mrb)
{
}

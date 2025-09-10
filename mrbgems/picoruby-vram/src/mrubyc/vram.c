#include <mrubyc.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "../../include/vram.h"

static void
mrbc_vram_free(mrb_value *self)
{
  display_t *disp = (display_t *)self->instance->data;
  for (int i = 0; i < disp->page_count; i++) {
    display_page_t *page = &disp->pages[i];
    if (page->buffer.string) {
      mrbc_string_delete(&page->buffer);
    }
  }
}

static void
mrubyc_mono_page_set_pixel(struct display_page *page, int x, int y, uint32_t color)
{
  if (x < 0 || x >= page->w || y < 0 || y >= page->h) return;

  unsigned char *data = (unsigned char *)page->buffer.string->data;
  int byte_idx = x + (y / 8) * page->w;
  int bit_idx = y % 8;

  if (byte_idx < 0 || byte_idx >= (int)page->buffer.string->size) return;

  if (color) {
    data[byte_idx] |= (1 << bit_idx);
  } else {
    data[byte_idx] &= ~(1 << bit_idx);
  }
  page->dirty = true;
}

static uint32_t
mrubyc_mono_page_get_pixel(struct display_page *page, int x, int y)
{
  if (x < 0 || x >= page->w || y < 0 || y >= page->h) return 0;

  unsigned char *data = (unsigned char *)page->buffer.string->data;
  int byte_idx = x + (y / 8) * page->w;
  int bit_idx = y % 8;

  if (byte_idx < 0 || byte_idx >= (int)page->buffer.string->size) return 0;

  return (data[byte_idx] >> bit_idx) & 1;
}

static void
mrubyc_mono_page_fill(struct display_page *page, uint32_t color)
{
  unsigned char *data = (unsigned char *)page->buffer.string->data;
  memset(data, color ? 0xFF : 0x00, page->buffer.string->size);
  page->dirty = true;
}

/*
 * VRAM.new(w: 128, h: 64, cols: 1, rows: 8)
 */
static void
c_vram_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value w_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("w")));
  mrbc_value h_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("h")));
  mrbc_value cols_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("cols")));
  mrbc_value rows_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("rows")));

  if (mrbc_type(w_val) != MRBC_TT_INTEGER || mrbc_type(h_val) != MRBC_TT_INTEGER ||
      mrbc_type(cols_val) != MRBC_TT_INTEGER || mrbc_type(rows_val) != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "Invalid arguments for VRAM.new");
    return;
  }

  int w = w_val.i;
  int h = h_val.i;
  int cols = cols_val.i;
  int rows = rows_val.i;
  int page_w = w / cols;
  int page_h = h / rows;

  mrbc_value vram = mrbc_instance_new(vm, v->cls, sizeof(display_t) + sizeof(display_page_t) * cols * rows);
  display_t *disp = (display_t *)vram.instance->data;
  if (!disp) {
    mrbc_raise(vm, MRBC_CLASS(NoMemoryError), "Failed to allocate display");
    return;
  }

  disp->w = w;
  disp->h = h;
  disp->page_count = rows * cols;

  int page_size = page_w * ((page_h + 7) / 8);

  for (int ty = 0; ty < rows; ty++) {
    for (int tx = 0; tx < cols; tx++) {
      display_page_t *page = &disp->pages[ty * cols + tx];
      page->x = tx * page_w;
      page->y = ty * page_h;
      page->w = page_w;
      page->h = page_h;

      // Create buffer string
      page->buffer = mrbc_string_new(vm, NULL, page_size);
      memset(page->buffer.string->data, 0, page_size);
      // TODO: memory leak of page->buffer happens

      page->dirty = false;
      page->pixel_format = PIXEL_FORMAT_MONO;
      page->set_pixel = mrubyc_mono_page_set_pixel;
      page->get_pixel = mrubyc_mono_page_get_pixel;
      page->fill = mrubyc_mono_page_fill;
      page->fill(page, 0);
    }
  }

  SET_RETURN(vram);
}

static mrbc_value
vram_pages_sub(mrbc_vm *vm, mrbc_value *v, int argc, bool dirty)
{
  bool clear_dirty = true;
  if (argc != 0 && mrbc_type(v[1]) == MRBC_TT_FALSE) {
    clear_dirty = false;
  }
  display_t *disp = (display_t *)v[0].instance->data;
  mrbc_value result = mrbc_array_new(vm, 0);

  if (!disp) {
    return result;
  }

  for (int i = 0; i < disp->page_count; i++) {
    display_page_t *page = &disp->pages[i];
    if (!dirty || page->dirty) {
      int cols = disp->w / page->w;
      int col = i % cols;
      int row = i / cols;
      mrbc_value entry = mrbc_array_new(vm, 3);
      mrbc_incref(&entry);
      mrbc_array_set(&entry, 0, &mrbc_integer_value(col));
      mrbc_array_set(&entry, 1, &mrbc_integer_value(row));
      mrbc_array_set(&entry, 2, &page->buffer);
      mrbc_array_push(&result, &entry);
      if (clear_dirty) page->dirty = false;
    }
  }
  return result;
}

/*
 * vram.pages
 */
static void
c_vram_pages(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value result = vram_pages_sub(vm, v, argc, false);
  SET_RETURN(result);
}

/*
 * vram.dirty_pages
 */
static void
c_vram_dirty_pages(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value result = vram_pages_sub(vm, v, argc, true);
  SET_RETURN(result);
}

/*
 * vram.set_pixel(x, y, color)
 */
static void
c_vram_set_pixel(mrbc_vm *vm, mrbc_value *v, int argc)
{
  display_t *disp = (display_t *)v[0].instance->data;
  if (!disp) return;

  int x = GET_INT_ARG(1);
  int y = GET_INT_ARG(2);
  int color = GET_INT_ARG(3);

  for (int i = 0; i < disp->page_count; i++) {
    display_page_t *page = &disp->pages[i];
    if (x >= page->x && x < page->x + page->w &&
        y >= page->y && y < page->y + page->h) {
      page->set_pixel(page, x - page->x, y - page->y, color);
      break;
    }
  }

  //mrbc_incref(&v[0]);
  //SET_RETURN(v[0]);
}

/*
 * vram.draw_line(x0, y0, x1, y1, color)
 */
static void
c_vram_draw_line(mrbc_vm *vm, mrbc_value *v, int argc)
{
  display_t *disp = (display_t *)v[0].instance->data;
  if (!disp) return;

  int x0 = GET_INT_ARG(1);
  int y0 = GET_INT_ARG(2);
  int x1 = GET_INT_ARG(3);
  int y1 = GET_INT_ARG(4);
  int color = GET_INT_ARG(5);

  // Bresenham's line algorithm
  int dx = x1 > x0 ? x1 - x0 : x0 - x1;
  int dy = y1 > y0 ? y1 - y0 : y0 - y1;
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx - dy;

  int x = x0;
  int y = y0;

  while (1) {
    // Set pixel at current position
    for (int i = 0; i < disp->page_count; i++) {
      display_page_t *page = &disp->pages[i];
      if (x >= page->x && x < page->x + page->w &&
          y >= page->y && y < page->y + page->h) {
        page->set_pixel(page, x - page->x, y - page->y, color);
        break;
      }
    }

    if (x == x1 && y == y1) break;

    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x += sx;
    }
    if (e2 < dx) {
      err += dx;
      y += sy;
    }
  }

  //mrbc_incref(&v[0]);
  //SET_RETURN(v[0]);
}

/*
 * vram.draw_rect(x, y, w, h, color)
 */
static void
c_vram_draw_rect(mrbc_vm *vm, mrbc_value *v, int argc)
{
  display_t *disp = (display_t *)v[0].instance->data;
  if (!disp) return;

  int x = GET_INT_ARG(1);
  int y = GET_INT_ARG(2);
  int w = GET_INT_ARG(3);
  int h = GET_INT_ARG(4);
  int color = GET_INT_ARG(5);

  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      int px = x + j;
      int py = y + i;
      for (int k = 0; k < disp->page_count; k++) {
        display_page_t *page = &disp->pages[k];
        if (px >= page->x && px < page->x + page->w &&
            py >= page->y && py < page->y + page->h) {
          page->set_pixel(page, px - page->x, py - page->y, color);
          break;
        }
      }
    }
  }

  //mrbc_incref(&v[0]);
  //SET_RETURN(v[0]);
}

/*
 * vram.fill(color)
 */
static void
c_vram_fill(mrbc_vm *vm, mrbc_value *v, int argc)
{
  display_t *disp = (display_t *)v[0].instance->data;
  if (!disp) return;

  int color = GET_INT_ARG(1);
  for (int i = 0; i < disp->page_count; i++) {
    display_page_t *page = &disp->pages[i];
    page->fill(page, color);
  }

  //mrbc_incref(&v[0]);
  //SET_RETURN(v[0]);
}

/*
 * vram.draw_bitmap(x:, y:, w:, h:, data:)
 * data: Array[Integer] - each element represents a row's bit pattern
 */
static void
c_vram_draw_bitmap(mrbc_vm *vm, mrbc_value *v, int argc)
{
  display_t *disp = (display_t *)v[0].instance->data;
  if (!disp) return;

  mrbc_value x_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("x")));
  mrbc_value y_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("y")));
  mrbc_value width_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("w")));
  mrbc_value height_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("h")));
  mrbc_value data_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("data")));

  if (mrbc_type(x_val) != MRBC_TT_INTEGER || mrbc_type(y_val) != MRBC_TT_INTEGER ||
      mrbc_type(width_val) != MRBC_TT_INTEGER || mrbc_type(height_val) != MRBC_TT_INTEGER ||
      mrbc_type(data_val) != MRBC_TT_ARRAY) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "Invalid arguments for draw_bitmap");
    return;
  }

  int x = x_val.i;
  int y = y_val.i;
  int width = width_val.i;
  int height = height_val.i;

  if (width <= 0 || height <= 0) return;

  for (int img_y = 0; img_y < height && img_y < data_val.array->n_stored; img_y++) {
    mrbc_value row_val = mrbc_array_get(&data_val, img_y);
    if (mrbc_type(row_val) == MRBC_TT_INTEGER) {
      uint64_t row_data = (uint64_t)row_val.i;
      for (int img_x = 0; img_x < width; img_x++) {
        uint8_t pixel = (row_data >> (width - 1 - img_x)) & 1;
        int screen_x = x + img_x;
        int screen_y = y + img_y;

        for (int i = 0; i < disp->page_count; i++) {
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
}

/*
 * vram.draw_bytes(x:, y:, w:, h:, data:)
 * data: String - packed byte array
 */
static void
c_vram_draw_bytes(mrbc_vm *vm, mrbc_value *v, int argc)
{
  display_t *disp = (display_t *)v[0].instance->data;
  if (!disp) return;

  mrbc_value x_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("x")));
  mrbc_value y_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("y")));
  mrbc_value width_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("w")));
  mrbc_value height_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("h")));
  mrbc_value data_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("data")));

  if (mrbc_type(x_val) != MRBC_TT_INTEGER || mrbc_type(y_val) != MRBC_TT_INTEGER ||
      mrbc_type(width_val) != MRBC_TT_INTEGER || mrbc_type(height_val) != MRBC_TT_INTEGER ||
      mrbc_type(data_val) != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "Invalid arguments for draw_bytes");
    return;
  }

  int x = x_val.i;
  int y = y_val.i;
  int width = width_val.i;
  int height = height_val.i;

  if (width <= 0 || height <= 0) return;

  const uint8_t *image_data = (const uint8_t *)data_val.string->data;
  int data_size = data_val.string->size;

  for (int img_y = 0; img_y < height; img_y++) {
    for (int img_x = 0; img_x < width; img_x++) {
      int byte_idx = (img_y * ((width + 7) / 8)) + (img_x / 8);
      int bit_idx = 7 - (img_x % 8);

      uint8_t pixel = 0;
      if (byte_idx < data_size) {
        pixel = (image_data[byte_idx] >> bit_idx) & 1;
      }

      int screen_x = x + img_x;
      int screen_y = y + img_y;

      for (int i = 0; i < disp->page_count; i++) {
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

/*
 * vram.erase(x, y, w, h)
 */
static void
c_vram_erase(mrbc_vm *vm, mrbc_value *v, int argc)
{
  display_t *disp = (display_t *)v[0].instance->data;
  if (!disp) return;

  int x = GET_INT_ARG(1);
  int y = GET_INT_ARG(2);
  int w = GET_INT_ARG(3);
  int h = GET_INT_ARG(4);

  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      int px = x + j;
      int py = y + i;
      for (int k = 0; k < disp->page_count; k++) {
        display_page_t *page = &disp->pages[k];
        if (px >= page->x && px < page->x + page->w &&
            py >= page->y && py < page->y + page->h) {
          page->set_pixel(page, px - page->x, py - page->y, 0);
          break;
        }
      }
    }
  }
}

void
mrbc_vram_init(struct VM *vm)
{
  mrbc_class *class_VRAM = mrbc_define_class(0, "VRAM", mrbc_class_object);
  mrbc_define_destructor(class_VRAM, mrbc_vram_free);

  mrbc_define_method(0, class_VRAM, "new", c_vram_new);
  mrbc_define_method(0, class_VRAM, "pages", c_vram_pages);
  mrbc_define_method(0, class_VRAM, "dirty_pages", c_vram_dirty_pages);
  mrbc_define_method(0, class_VRAM, "set_pixel", c_vram_set_pixel);
  mrbc_define_method(0, class_VRAM, "draw_line", c_vram_draw_line);
  mrbc_define_method(0, class_VRAM, "draw_rect", c_vram_draw_rect);
  mrbc_define_method(0, class_VRAM, "draw_bitmap", c_vram_draw_bitmap);
  mrbc_define_method(0, class_VRAM, "draw_bytes", c_vram_draw_bytes);
  mrbc_define_method(0, class_VRAM, "fill", c_vram_fill);
  mrbc_define_method(0, class_VRAM, "erase", c_vram_erase);
}

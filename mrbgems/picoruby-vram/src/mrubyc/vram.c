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

/*
 * VRAM.new(w: 128, h: 64, cols: 1, rows: 8)
 * VRAM.new(w: 200, h: 200, cols: 1, rows: 1, layout: :horizontal, invert: true)
 */
static void
c_vram_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value w_val    = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("w")));
  mrbc_value h_val    = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("h")));
  mrbc_value cols_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("cols")));
  mrbc_value rows_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("rows")));

  if (mrbc_type(w_val) != MRBC_TT_INTEGER || mrbc_type(h_val) != MRBC_TT_INTEGER ||
      mrbc_type(cols_val) != MRBC_TT_INTEGER || mrbc_type(rows_val) != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "Invalid arguments for VRAM.new");
    return;
  }

  /* Optional: layout (:vertical default, :horizontal for UC8151) */
  mrbc_value layout_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("layout")));
  bool horizontal = (mrbc_type(layout_val) == MRBC_TT_SYMBOL) &&
                    (layout_val.i == (int32_t)mrbc_str_to_symid("horizontal"));

  /* Optional: invert (false default) */
  mrbc_value invert_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("invert")));
  bool invert = (mrbc_type(invert_val) == MRBC_TT_TRUE);

  /* Optional: rotate (0 default, clockwise degrees: 0/90/180/270) */
  mrbc_value rotate_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("rotate")));
  int rotate = (mrbc_type(rotate_val) == MRBC_TT_INTEGER) ? rotate_val.i : 0;

  int w    = w_val.i;
  int h    = h_val.i;
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

  size_t page_size = display_page_buffer_size(page_w, page_h, horizontal, rotate);

  for (int ty = 0; ty < rows; ty++) {
    for (int tx = 0; tx < cols; tx++) {
      display_page_t *page = &disp->pages[ty * cols + tx];
      page->x = tx * page_w;
      page->y = ty * page_h;
      page->w = page_w;
      page->h = page_h;
      page->invert = invert;
      page->rotate = rotate;

      /* Create buffer string */
      page->buffer = mrbc_string_new(vm, NULL, (int)page_size);
      memset(page->buffer.string->data, 0, page_size);
      page->raw_data = (uint8_t *)page->buffer.string->data;
      page->raw_size = page_size;
      /* TODO: memory leak of page->buffer happens */

      page->dirty = false;
      display_page_init_format(page, horizontal);
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
  display_set_pixel(disp, GET_INT_ARG(1), GET_INT_ARG(2), (uint32_t)GET_INT_ARG(3));
}

/*
 * vram.draw_line(x0, y0, x1, y1, color)
 */
static void
c_vram_draw_line(mrbc_vm *vm, mrbc_value *v, int argc)
{
  display_t *disp = (display_t *)v[0].instance->data;
  if (!disp) return;
  display_draw_line(disp,
                    GET_INT_ARG(1), GET_INT_ARG(2),
                    GET_INT_ARG(3), GET_INT_ARG(4),
                    (uint32_t)GET_INT_ARG(5));
}

/*
 * vram.draw_rect(x, y, w, h, color)
 */
static void
c_vram_draw_rect(mrbc_vm *vm, mrbc_value *v, int argc)
{
  display_t *disp = (display_t *)v[0].instance->data;
  if (!disp) return;
  display_draw_rect(disp,
                    GET_INT_ARG(1), GET_INT_ARG(2),
                    GET_INT_ARG(3), GET_INT_ARG(4),
                    (uint32_t)GET_INT_ARG(5));
}

/*
 * vram.fill(color)
 */
static void
c_vram_fill(mrbc_vm *vm, mrbc_value *v, int argc)
{
  display_t *disp = (display_t *)v[0].instance->data;
  if (!disp) return;
  display_fill(disp, (uint32_t)GET_INT_ARG(1));
}

/* draw_bitmap: VM-specific because it takes an Array[Integer] */
static void
c_vram_draw_bitmap(mrbc_vm *vm, mrbc_value *v, int argc)
{
  display_t *disp = (display_t *)v[0].instance->data;
  if (!disp) return;

  mrbc_value x_val      = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("x")));
  mrbc_value y_val      = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("y")));
  mrbc_value width_val  = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("w")));
  mrbc_value height_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("h")));
  mrbc_value data_val   = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("data")));

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
        display_set_pixel(disp, x + img_x, y + img_y, pixel);
      }
    }
  }
}

/*
 * vram.draw_bytes(x:, y:, w:, h:, data:)
 */
static void
c_vram_draw_bytes(mrbc_vm *vm, mrbc_value *v, int argc)
{
  display_t *disp = (display_t *)v[0].instance->data;
  if (!disp) return;

  mrbc_value x_val      = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("x")));
  mrbc_value y_val      = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("y")));
  mrbc_value width_val  = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("w")));
  mrbc_value height_val = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("h")));
  mrbc_value data_val   = mrbc_hash_get(v + 1, &mrbc_symbol_value(mrbc_str_to_symid("data")));

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

  display_draw_bytes(disp, x, y, width, height,
                     (const uint8_t *)data_val.string->data,
                     (size_t)data_val.string->size);
}

/*
 * vram.erase(x, y, w, h)
 */
static void
c_vram_erase(mrbc_vm *vm, mrbc_value *v, int argc)
{
  display_t *disp = (display_t *)v[0].instance->data;
  if (!disp) return;
  display_erase(disp, GET_INT_ARG(1), GET_INT_ARG(2), GET_INT_ARG(3), GET_INT_ARG(4));
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

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

/*
 * vram = VRAM.new(w: 128, h: 64, cols: 1, rows: 8)
 * vram = VRAM.new(w: 200, h: 200, cols: 1, rows: 1, layout: :horizontal, invert: true)
 */
static mrb_value
mrb_vram_s_new(mrb_state* mrb, mrb_value klass)
{
  const mrb_sym kw_names[] = {
    MRB_SYM(w), MRB_SYM(h), MRB_SYM(cols), MRB_SYM(rows),
    MRB_SYM(layout), MRB_SYM(invert), MRB_SYM(rotate)
  };
  mrb_value kw_values[7];
  mrb_value kw_rest;
  mrb_kwargs kwargs = { 7, 4, kw_names, kw_values, &kw_rest };
  mrb_get_args(mrb, ":", &kwargs);

  mrb_int w    = mrb_fixnum(kw_values[0]);
  mrb_int h    = mrb_fixnum(kw_values[1]);
  mrb_int cols = mrb_fixnum(kw_values[2]);
  mrb_int rows = mrb_fixnum(kw_values[3]);

  /* Optional: layout (:vertical default, :horizontal for UC8151) */
  bool horizontal = false;
  if (!mrb_undef_p(kw_values[4]) && mrb_symbol_p(kw_values[4])) {
    if (mrb_symbol(kw_values[4]) == mrb_intern_lit(mrb, "horizontal")) {
      horizontal = true;
    }
  }

  /* Optional: invert (false default) */
  bool invert = false;
  if (!mrb_undef_p(kw_values[5])) {
    invert = mrb_test(kw_values[5]);
  }

  /* Optional: rotate (0 default, clockwise degrees: 0/90/180/270) */
  int rotate = 0;
  if (!mrb_undef_p(kw_values[6]) && mrb_integer_p(kw_values[6])) {
    rotate = (int)mrb_integer(kw_values[6]);
  }

  mrb_int page_w = w / cols;
  mrb_int page_h = h / rows;

  display_t *disp = mrb_malloc(mrb, sizeof(display_t));
  disp->w = w;
  disp->h = h;
  disp->page_count = rows * cols;
  disp->pages = mrb_malloc(mrb, sizeof(display_page_t) * disp->page_count);

  mrb_value vram = mrb_obj_value(Data_Wrap_Struct(mrb, mrb_class_ptr(klass), &mrb_vram_type, disp));

  size_t page_size = display_page_buffer_size(page_w, page_h, horizontal, rotate);

  for (mrb_int ty = 0; ty < rows; ty++) {
    for (mrb_int tx = 0; tx < cols; tx++) {
      display_page_t *page = &disp->pages[ty * cols + tx];
      page->x = tx * page_w;
      page->y = ty * page_h;
      page->w = page_w;
      page->h = page_h;
      page->invert = invert;
      page->rotate = rotate;
      page->buffer = mrb_str_new(mrb, NULL, (mrb_int)page_size);
      char *data = RSTRING_PTR(page->buffer);
      memset(data, 0, page_size);
      page->raw_data = (uint8_t *)data;
      page->raw_size = page_size;
      mrb_gc_register(mrb, page->buffer);
      page->dirty = false;
      display_page_init_format(page, horizontal);
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
  display_draw_line(disp, x0, y0, x1, y1, (uint32_t)color);
  return self;
}

static mrb_value
mrb_vram_draw_rect(mrb_state* mrb, mrb_value self)
{
  mrb_int x, y, w, h, color;
  mrb_get_args(mrb, "iiiii", &x, &y, &w, &h, &color);
  display_t *disp = (display_t *)mrb_data_get_ptr(mrb, self, &mrb_vram_type);
  display_draw_rect(disp, x, y, w, h, (uint32_t)color);
  return self;
}

static mrb_value
mrb_vram_set_pixel(mrb_state* mrb, mrb_value self)
{
  mrb_int x, y, color;
  mrb_get_args(mrb, "iii", &x, &y, &color);
  display_t *disp = (display_t *)mrb_data_get_ptr(mrb, self, &mrb_vram_type);
  display_set_pixel(disp, x, y, (uint32_t)color);
  return self;
}

static mrb_value
mrb_vram_fill(mrb_state* mrb, mrb_value self)
{
  mrb_int color;
  mrb_get_args(mrb, "i", &color);
  display_t *disp = (display_t *)mrb_data_get_ptr(mrb, self, &mrb_vram_type);
  display_fill(disp, (uint32_t)color);
  return self;
}

static mrb_value
mrb_vram_erase(mrb_state* mrb, mrb_value self)
{
  mrb_int x, y, w, h;
  mrb_get_args(mrb, "iiii", &x, &y, &w, &h);
  display_t *disp = (display_t *)mrb_data_get_ptr(mrb, self, &mrb_vram_type);
  display_erase(disp, x, y, w, h);
  return self;
}

/* draw_bitmap: VM-specific because it takes an Array[Integer] */
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
        display_set_pixel(disp, x + img_x, y + img_y, pixel);
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

  display_draw_bytes(disp, x, y, width, height,
                     (const uint8_t *)RSTRING_PTR(data_val),
                     (size_t)RSTRING_LEN(data_val));
  return self;
}

void
mrb_picoruby_vram_gem_init(mrb_state* mrb)
{
  struct RClass *class_VRAM = mrb_define_class_id(mrb, MRB_SYM(VRAM), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_VRAM, MRB_TT_CDATA);

  mrb_define_class_method_id(mrb, class_VRAM, MRB_SYM(new), mrb_vram_s_new, MRB_ARGS_KEY(4, 3));
  mrb_define_method_id(mrb, class_VRAM, MRB_SYM(pages), mrb_vram_pages, MRB_ARGS_OPT(1));
  mrb_define_method_id(mrb, class_VRAM, MRB_SYM(dirty_pages), mrb_vram_dirty_pages, MRB_ARGS_OPT(1));
  mrb_define_method_id(mrb, class_VRAM, MRB_SYM(draw_line), mrb_vram_draw_line, MRB_ARGS_REQ(5));
  mrb_define_method_id(mrb, class_VRAM, MRB_SYM(draw_rect), mrb_vram_draw_rect, MRB_ARGS_REQ(5));
  mrb_define_method_id(mrb, class_VRAM, MRB_SYM(draw_bitmap), mrb_vram_draw_bitmap, MRB_ARGS_KEY(5, 5));
  mrb_define_method_id(mrb, class_VRAM, MRB_SYM(draw_bytes), mrb_vram_draw_bytes, MRB_ARGS_KEY(5, 5));
  mrb_define_method_id(mrb, class_VRAM, MRB_SYM(set_pixel), mrb_vram_set_pixel, MRB_ARGS_REQ(3));
  mrb_define_method_id(mrb, class_VRAM, MRB_SYM(fill), mrb_vram_fill, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_VRAM, MRB_SYM(erase), mrb_vram_erase, MRB_ARGS_REQ(4));
}

void
mrb_picoruby_vram_gem_final(mrb_state* mrb)
{
}

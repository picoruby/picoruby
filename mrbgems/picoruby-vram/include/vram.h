#ifndef VRAM_DEFINED_H_
#define VRAM_DEFINED_H_

#include "picoruby.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  PIXEL_FORMAT_MONO,        /*  1bit/pixel, vertical layout (SSD1306 style) */
  PIXEL_FORMAT_MONO_H,      /*  1bit/pixel, horizontal layout (UC8151 style) */
  PIXEL_FORMAT_GRAY4,       /*  4bit/pixel */
  PIXEL_FORMAT_GRAY8,       /*  8bit/pixel */
  PIXEL_FORMAT_RGB565,      /* 16bit/pixel */
  PIXEL_FORMAT_RGB888,      /* 24bit/pixel */
  PIXEL_FORMAT_ARGB8888,    /* 32bit/pixel */
} pixel_format_t;

/* Abstract Display Page Structure */
typedef struct display_page {
  int page_id;
  int x, y;          /* Offset position in the overall display */
  int w, h;          /* Logical pixel size (user-facing coordinates) */
  int buf_w, buf_h;  /* Buffer pixel size (swapped from w,h when rotate=90/270) */
  int rotate;        /* Clockwise rotation in degrees: 0, 90, 180, 270 */
  pixel_format_t pixel_format;
  bool invert;       /* When true, flip bit polarity on set/get/fill */
  uint8_t *raw_data; /* Direct pointer into VM-managed buffer data */
  size_t raw_size;   /* Byte count of raw_data */
#if defined(PICORB_VM_MRUBY)
  mrb_value buffer;   /* Pixel data */
#elif defined(PICORB_VM_MRUBYC)
  mrbc_value buffer;  /* Pixel data */
#endif
  bool  dirty;
  size_t buffer_size;
  void (*set_pixel)(struct display_page*, int x, int y, uint32_t color);
  uint32_t (*get_pixel)(struct display_page*, int x, int y);
  void (*fill)(struct display_page*, uint32_t color);
} display_page_t;

typedef struct display {
  int w;
  int h;
  int page_count;
#if defined(PICORB_VM_MRUBY)
  display_page_t *pages;
#elif defined(PICORB_VM_MRUBYC)
  display_page_t pages[];
#endif
} display_t;

#ifdef __cplusplus
}
#endif

#endif /* VRAM_DEFINED_H_ */

#include <stdlib.h>
#include <stdbool.h>
#include <mrubyc.h>
#include "../include/prk-rgb.h"

#define MAX_PIXEL_SIZE 150

static int32_t pixels[MAX_PIXEL_SIZE + 1] = {};
static int32_t dma_ws2812_grb_pixels[MAX_PIXEL_SIZE];
static int     dma_ws2812_channel = -1;
static uint8_t dma_ws2812_last_append_index = 0;
static int     swirl_index = 0;

static void
c_ws2812_init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  bool is_rgbw;
  if (GET_ARG(3).tt == MRBC_TT_TRUE) {
    is_rgbw = true;
  } else {
    is_rgbw = false;
  }
  RGB_init_pio(GET_INT_ARG(1), is_rgbw);

  dma_ws2812_channel = RGB_init_dma_ws2812(dma_ws2812_channel, GET_INT_ARG(2), &dma_ws2812_grb_pixels);
}

static void
c_ws2812_set_pixel_at(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pixels[(uint8_t)GET_INT_ARG(1)] = GET_INT_ARG(2);
}

static inline void
show_pixels(void)
{
  RGB_dma_channel_set_read_addr(dma_ws2812_channel, dma_ws2812_grb_pixels, true);
  dma_ws2812_last_append_index = 0;
}

static inline uint32_t
rgb2grb(int32_t val)
{
  return (uint32_t)((val & 0xff0000) >> 8) |
         (uint32_t)((val & 0x00ff00) << 8) |
         (uint32_t) (val & 0x0000ff);
}

static inline void
put_pixel(int32_t pixel_rgb)
{
  dma_ws2812_grb_pixels[dma_ws2812_last_append_index++] = rgb2grb(pixel_rgb) << 8u;
}

static void
c_ws2812_fill(mrbc_vm *vm, mrbc_value *v, int argc)
{
  for (int i = 0; i < GET_INT_ARG(2); i++) {
    put_pixel(GET_INT_ARG(1));
  }
  show_pixels();
}

static void
c_ws2812_rotate_swirl(mrbc_vm *vm, mrbc_value *v, int argc)
{
  volatile int at = swirl_index;
  int stop = GET_INT_ARG(1);
  for (int i = 0; i < stop; i++) {
    put_pixel(pixels[at]);
    at++;
    if (pixels[at] < 0) at = 0;
  }
  swirl_index++;
  if (pixels[swirl_index] < 0) {
    swirl_index = 0;
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
  show_pixels();
}

static void
c_ws2812_reset_swirl_index(mrbc_vm *vm, mrbc_value *v, int argc)
{
  swirl_index = 0;
}

static void
c_ws2812_show(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int i = 0;
  while (pixels[i] >= 0) {
    put_pixel((uint32_t)pixels[i]);
    i++;
  }
  show_pixels();
}

static void
c_ws2812_rand_show(mrbc_vm *vm, mrbc_value *v, int argc)
{
  for (int i = 0; i < GET_INT_ARG(3); i++) {
    if (rand() % 16 > GET_INT_ARG(2)) {
      put_pixel(GET_INT_ARG(1));
    } else {
      put_pixel(0);
    }
    i++;
  }
  show_pixels();
}


/******************************************************
 *
 * Functions and variables for RGB MATRIX
 *
 ******************************************************/

static uint8_t  matrix_coordinate[MAX_PIXEL_SIZE][2];
static uint32_t pixel_distance[MAX_PIXEL_SIZE];
static uint8_t  circle_center[2] = { 112, 32 };

static void
c_ws2812_add_matrix_pixel_at(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint16_t i = GET_INT_ARG(1);
  matrix_coordinate[i][0] = (uint8_t)GET_INT_ARG(2);
  matrix_coordinate[i][1] = (uint8_t)GET_INT_ARG(3);
}

static void
c_ws2812_circle_set_center(mrbc_vm *vm, mrbc_value *v, int argc)
{
  circle_center[0] = GET_INT_ARG(1);
  circle_center[1] = GET_INT_ARG(2);
}

static void
c_ws2812_init_pixel_distance(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int32_t x;
  int32_t y;
  for (int i=0; i < GET_INT_ARG(1); i++) {
    x = matrix_coordinate[i][0] - circle_center[0];
    y = (matrix_coordinate[i][1] - circle_center[1]) * 3;
    pixel_distance[i] = x * x + y * y;
  }
}

static inline uint8_t
get_distance_group(uint32_t d)
{
  if(d < 400) {
    // 20
    return 0;
  } else if (d < 1600) {
    // 40
    return 1;
  } else if (d < 3600) {
    // 60
    return 2;
  } else if (d < 6400) {
    // 80
    return 3;
  } else if (d < 10000) {
    // 100
    return 4;
  } else if (d < 14400) {
    // 120
    return 5;
  } else if (d < 19600) {
    // 140
    return 6;
  } else if (d < 25600) {
    // 160
    return 7;
  } else if (d < 32800) {
    // 180
    return 8;
  } else if (d < 40000) {
    // 200
    return 9;
  } else if (d < 48400) {
    // 220
    return 10;
  } else if (d < 57600) {
    // 240
    return 11;
  } else {
    return 12;
  }
}

#define RGB(r,g,b) ( r<<16 | g<<8 | b )

static void
c_ws2812_circle(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int count = GET_INT_ARG(1);
  int value = GET_INT_ARG(2);
  int circle_diameter = GET_INT_ARG(3);
  uint8_t lv = value * 2;
  for(uint16_t i = 0; i < count; i++) {
    uint8_t b = get_distance_group(pixel_distance[i]);
    b = (b + circle_diameter) % 12;
    switch(b) {
      case 0:
        put_pixel(RGB(lv * 4, 0 ,0 ));
        break;
      case 1:
        put_pixel(RGB(lv * 3, lv, 0));
        break;
      case 2:
        put_pixel(RGB(lv * 2, lv * 2, 0));
        break;
      case 3:
        put_pixel(RGB(lv, lv * 3, 0));
        break;
      case 4:
        put_pixel(RGB(0, lv * 4, 0));
        break;
      case 5:
        put_pixel(RGB(0, lv * 3, lv) );
        break;
      case 6:
        put_pixel(RGB(0, lv * 2, lv * 2));
        break;
      case 7:
        put_pixel(RGB(0, lv, lv * 3));
        break;
      case 8:
        put_pixel(RGB(0, 0, lv * 4));
        break;
      case 9:
        put_pixel(RGB(lv, 0, lv * 3));
        break;
      case 10:
        put_pixel(RGB(lv * 2, 0, lv * 2) );
        break;
      case 11:
        put_pixel(RGB(lv * 3, 0, lv));
        break;
      default:
        put_pixel(RGB(lv, lv, lv));
        break;
    }
  }
  show_pixels();
}

void
mrbc_prk_rgb_init(void)
{
  mrbc_class *mrbc_class_RGB = mrbc_define_class(0, "RGB", mrbc_class_object);
  mrbc_define_method(0, mrbc_class_RGB, "ws2812_init",                c_ws2812_init);
  mrbc_define_method(0, mrbc_class_RGB, "ws2812_show",                c_ws2812_show);
  mrbc_define_method(0, mrbc_class_RGB, "ws2812_fill",                c_ws2812_fill);
  mrbc_define_method(0, mrbc_class_RGB, "ws2812_rand_show",           c_ws2812_rand_show);
  mrbc_define_method(0, mrbc_class_RGB, "ws2812_set_pixel_at",        c_ws2812_set_pixel_at);
  mrbc_define_method(0, mrbc_class_RGB, "ws2812_rotate_swirl",        c_ws2812_rotate_swirl);
  mrbc_define_method(0, mrbc_class_RGB, "ws2812_reset_swirl_index",   c_ws2812_reset_swirl_index);
  mrbc_define_method(0, mrbc_class_RGB, "ws2812_add_matrix_pixel_at", c_ws2812_add_matrix_pixel_at);
  mrbc_define_method(0, mrbc_class_RGB, "ws2812_init_pixel_distance", c_ws2812_init_pixel_distance);
  mrbc_define_method(0, mrbc_class_RGB, "ws2812_circle",              c_ws2812_circle);
  mrbc_define_method(0, mrbc_class_RGB, "ws2812_circle_set_center",   c_ws2812_circle_set_center);

  RGB_init_sub(mrbc_class_RGB);
}


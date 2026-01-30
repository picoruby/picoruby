/*
 * Copyright (c) 2025 HASUMI Hitoshi | MIT License
 */

#include <mruby.h>
#include <mruby/data.h>
#include <mruby/class.h>
#include <mruby/variable.h>
#include <mruby/presym.h>

#include "../../include/usb-hid.h"

// External function from src/usb_hid.c
extern uint8_t usb_hid_keyboard_get_led_state(void);

//================================================================
// mruby methods
//================================================================

static mrb_value c_keyboard_send(mrb_state *mrb, mrb_value self)
{
  mrb_int modifier, keycode;
  mrb_get_args(mrb, "ii", &modifier, &keycode);

  uint8_t kc = (uint8_t)keycode;
  bool result = usb_hid_keyboard_send((uint8_t)modifier, &kc, 1);
  return mrb_bool_value(result);
}

static mrb_value c_keyboard_ready(mrb_state *mrb, mrb_value self)
{
  bool result = usb_hid_keyboard_ready();
  return mrb_bool_value(result);
}

static mrb_value c_keyboard_release(mrb_state *mrb, mrb_value self)
{
  bool result = usb_hid_keyboard_release_all();
  return mrb_bool_value(result);
}

static mrb_value c_keyboard_led_state(mrb_state *mrb, mrb_value self)
{
  uint8_t state = usb_hid_keyboard_get_led_state();
  return mrb_fixnum_value(state);
}

static mrb_value c_mouse_move(mrb_state *mrb, mrb_value self)
{
  mrb_int x, y, wheel, buttons;
  mrb_get_args(mrb, "iiii", &x, &y, &wheel, &buttons);

  bool result = usb_hid_mouse_send((int8_t)x, (int8_t)y, (int8_t)wheel, (uint8_t)buttons);
  return mrb_bool_value(result);
}

static mrb_value c_mouse_ready(mrb_state *mrb, mrb_value self)
{
  bool result = usb_hid_mouse_ready();
  return mrb_bool_value(result);
}

static mrb_value c_media_send(mrb_state *mrb, mrb_value self)
{
  mrb_int code;
  mrb_get_args(mrb, "i", &code);

  bool result = usb_hid_consumer_send((uint16_t)code);
  return mrb_bool_value(result);
}

static mrb_value c_media_ready(mrb_state *mrb, mrb_value self)
{
  bool result = usb_hid_consumer_ready();
  return mrb_bool_value(result);
}

//================================================================
// Initialize
//================================================================

void mrb_picoruby_usb_hid_gem_init(mrb_state *mrb)
{
  struct RClass *usb_hid_class = mrb_define_class_id(mrb, MRB_SYM(UsbHid), mrb->object_class);

  mrb_define_class_method_id(mrb, usb_hid_class, MRB_SYM(keyboard_send), c_keyboard_send, MRB_ARGS_REQ(2));
  mrb_define_class_method_id(mrb, usb_hid_class, MRB_SYM(keyboard_ready), c_keyboard_ready, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, usb_hid_class, MRB_SYM(keyboard_release), c_keyboard_release, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, usb_hid_class, MRB_SYM(keyboard_led_state), c_keyboard_led_state, MRB_ARGS_NONE());

  mrb_define_class_method_id(mrb, usb_hid_class, MRB_SYM(mouse_move), c_mouse_move, MRB_ARGS_REQ(4));
  mrb_define_class_method_id(mrb, usb_hid_class, MRB_SYM(mouse_ready), c_mouse_ready, MRB_ARGS_NONE());

  mrb_define_class_method_id(mrb, usb_hid_class, MRB_SYM(media_send), c_media_send, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, usb_hid_class, MRB_SYM(media_ready), c_media_ready, MRB_ARGS_NONE());

  // Include auto-generated keycode definitions
#include "keycode.inc"
}

void mrb_picoruby_usb_hid_gem_final(mrb_state *mrb)
{
  // Nothing to finalize
}

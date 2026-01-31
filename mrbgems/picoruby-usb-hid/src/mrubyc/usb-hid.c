/*
 * Copyright (c) 2025 HASUMI Hitoshi | MIT License
 */

#include <mrubyc.h>

#include "../../include/usb-hid.h"

// External function from ports/rp2040/usb-hid.c
extern uint8_t usb_hid_keyboard_get_led_state(void);

//================================================================
// mruby/c methods
//================================================================

static void
c_keyboard_send(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint8_t modifier = GET_INT_ARG(1);
  uint8_t keycode = GET_INT_ARG(2);

  bool result = usb_hid_keyboard_send(modifier, &keycode, 1);
  if (result) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_keyboard_ready(mrbc_vm *vm, mrbc_value *v, int argc)
{
  bool result = usb_hid_keyboard_ready();
  if (result) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_keyboard_release(mrbc_vm *vm, mrbc_value *v, int argc)
{
  bool result = usb_hid_keyboard_release_all();
  if (result) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_keyboard_led_state(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint8_t state = usb_hid_keyboard_get_led_state();
  SET_INT_RETURN(state);
}

static void
c_mouse_move(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int8_t x = GET_INT_ARG(1);
  int8_t y = GET_INT_ARG(2);
  int8_t wheel = GET_INT_ARG(3);
  uint8_t buttons = GET_INT_ARG(4);

  bool result = usb_hid_mouse_send(x, y, wheel, buttons);
  if (result) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_mouse_ready(mrbc_vm *vm, mrbc_value *v, int argc)
{
  bool result = usb_hid_mouse_ready();
  if (result) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_media_send(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint16_t code = GET_INT_ARG(1);

  bool result = usb_hid_consumer_send(code);
  if (result) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_media_ready(mrbc_vm *vm, mrbc_value *v, int argc)
{
  bool result = usb_hid_consumer_ready();
  if (result) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

//================================================================
// Initialize
//================================================================

#define SET_CLASS_CONST(klass, cst, val) \
  mrbc_set_class_const(klass, mrbc_str_to_symid(#cst), &mrbc_integer_value(val))

void
mrbc_usb_hid_init(mrbc_vm *vm)
{
  mrbc_class *mrbc_module_USB = mrbc_define_module(vm, "USB");
  mrbc_class *mrbc_class_HID = mrbc_define_class_under(vm, mrbc_module_USB, "HID", mrbc_class_object);

  mrbc_define_method(vm, mrbc_class_HID, "keyboard_send", c_keyboard_send);
  mrbc_define_method(vm, mrbc_class_HID, "keyboard_ready", c_keyboard_ready);
  mrbc_define_method(vm, mrbc_class_HID, "keyboard_release", c_keyboard_release);
  mrbc_define_method(vm, mrbc_class_HID, "keyboard_led_state", c_keyboard_led_state);

  mrbc_define_method(vm, mrbc_class_HID, "mouse_move", c_mouse_move);
  mrbc_define_method(vm, mrbc_class_HID, "mouse_ready", c_mouse_ready);

  mrbc_define_method(vm, mrbc_class_HID, "media_send", c_media_send);
  mrbc_define_method(vm, mrbc_class_HID, "media_ready", c_media_ready);

  mrbc_class *mrbc_module_Keycode = mrbc_define_module_under(vm, mrbc_class_HID, "Keycode");
  // Include auto-generated keycode definitions
#include "keycode.inc"
}

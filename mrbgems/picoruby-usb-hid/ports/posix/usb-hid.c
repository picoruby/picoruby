/*
 * Copyright (c) 2025 HASUMI Hitoshi | MIT License
 *
 * POSIX port: no USB device. Every report is accepted and dropped, so
 * that the Ruby side (and its tests) can run on the host.
 */

#include "../../include/usb-hid.h"

bool
usb_hid_keyboard_send(uint8_t modifier, const uint8_t* keycode, uint8_t keycode_count)
{
  (void)modifier;
  (void)keycode;
  (void)keycode_count;
  return true;
}

bool
usb_hid_keyboard_ready(void)
{
  return true;
}

bool
usb_hid_keyboard_release_all(void)
{
  return true;
}

uint8_t
usb_hid_keyboard_get_led_state(void)
{
  return 0;
}

bool
usb_hid_mouse_send(int8_t x, int8_t y, int8_t wheel, uint8_t buttons)
{
  (void)x;
  (void)y;
  (void)wheel;
  (void)buttons;
  return true;
}

bool
usb_hid_mouse_ready(void)
{
  return true;
}

bool
usb_hid_consumer_send(uint16_t usage_code)
{
  (void)usage_code;
  return true;
}

bool
usb_hid_consumer_ready(void)
{
  return true;
}

/*
 * Copyright (c) 2025 HASUMI Hitoshi | MIT License
 */

#include <stdint.h>
#include <stdbool.h>
#include <pico/stdlib.h>
#include <tusb.h>

#include "../../include/usb-hid.h"

// HID instance definitions
#define ITF_HID_KEYBOARD  0
#define ITF_HID_MOUSE     1
#define ITF_HID_CONSUMER  2

// Keyboard LED state (for CapsLock, NumLock, etc.)
static uint8_t keyboard_led_state = 0;

//================================================================
// TinyUSB callbacks (required by TinyUSB)
//================================================================

// Invoked when received GET_REPORT control request
uint16_t
tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                      hid_report_type_t report_type,
                      uint8_t* buffer, uint16_t reqlen)
{
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request
void
tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                      hid_report_type_t report_type,
                      uint8_t const* buffer, uint16_t bufsize)
{
  if (instance == ITF_HID_KEYBOARD &&
      report_type == HID_REPORT_TYPE_OUTPUT)
  {
    // Handle keyboard LED indicators (NumLock, CapsLock, ScrollLock)
    if (bufsize >= 1)
    {
      keyboard_led_state = buffer[0];
      // TODO: Update physical LEDs if connected
    }
  }
}

// Invoked when sent REPORT successfully to host
void
tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report,
                            uint16_t len)
{
  (void) instance;
  (void) report;
  (void) len;
}

//================================================================
// Public API functions (RP2040-specific implementation)
//================================================================

// Get keyboard LED state (for use by keyboard matrix scanner)
uint8_t
usb_hid_keyboard_get_led_state(void)
{
  return keyboard_led_state;
}

bool
usb_hid_keyboard_send(uint8_t modifier, const uint8_t* keycode, uint8_t keycode_count)
{
  if (!tud_hid_n_ready(ITF_HID_KEYBOARD)) {
    return false;
  }

  uint8_t report[8] = {0};
  report[0] = modifier;

  for (uint8_t i = 0; i < keycode_count && i < 6; i++) {
    report[2 + i] = keycode[i];
  }

  return tud_hid_n_keyboard_report(ITF_HID_KEYBOARD, 0, modifier, report + 2);
}

bool
usb_hid_keyboard_ready(void)
{
  return tud_hid_n_ready(ITF_HID_KEYBOARD);
}

bool
usb_hid_keyboard_release_all(void)
{
  if (!tud_hid_n_ready(ITF_HID_KEYBOARD)) {
    return false;
  }

  uint8_t empty[6] = {0};
  return tud_hid_n_keyboard_report(ITF_HID_KEYBOARD, 0, 0, empty);
}

bool
usb_hid_mouse_send(int8_t x, int8_t y, int8_t wheel, uint8_t buttons)
{
  if (!tud_hid_n_ready(ITF_HID_MOUSE)) {
    return false;
  }

  return tud_hid_n_mouse_report(ITF_HID_MOUSE, 0, buttons, x, y, wheel, 0);
}

bool
usb_hid_mouse_ready(void)
{
  return tud_hid_n_ready(ITF_HID_MOUSE);
}

bool
usb_hid_consumer_send(uint16_t usage_code)
{
  if (!tud_hid_n_ready(ITF_HID_CONSUMER)) {
    return false;
  }

  return tud_hid_n_report(ITF_HID_CONSUMER, 0, &usage_code, 2);
}

bool
usb_hid_consumer_ready(void)
{
  return tud_hid_n_ready(ITF_HID_CONSUMER);
}

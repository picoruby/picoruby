/*
 * Copyright (c) 2025 HASUMI Hitoshi | MIT License
 */

#ifndef USB_HID_DEFINED_H_
#define USB_HID_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Keyboard functions
bool usb_hid_keyboard_send(uint8_t modifier, const uint8_t* keycode, uint8_t keycode_count);
bool usb_hid_keyboard_ready(void);
bool usb_hid_keyboard_release_all(void);
uint8_t usb_hid_keyboard_get_led_state(void);

// Mouse functions
bool usb_hid_mouse_send(int8_t x, int8_t y, int8_t wheel, uint8_t buttons);
bool usb_hid_mouse_ready(void);

// Consumer Control functions (media keys)
bool usb_hid_consumer_send(uint16_t usage_code);
bool usb_hid_consumer_ready(void);

#ifdef __cplusplus
}
#endif

#endif

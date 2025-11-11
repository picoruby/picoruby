# picoruby-prk-keyboard

PRK (PicoRuby Keyboard) firmware - Main keyboard library for building custom keyboards with PicoRuby.

## Overview

This is the core library for PRK Firmware, enabling you to create custom mechanical keyboard firmware using Ruby.

## Basic Usage

```ruby
require 'keyboard'

kbd = Keyboard.new

# Define keyboard matrix
kbd.init_matrix_pins(
  rows: [6, 7, 8, 9],
  cols: [10, 11, 12, 13, 14, 15]
)

# Define layers
kbd.add_layer :default, [
  %i(KC_Q    KC_W    KC_E    KC_R    KC_T    KC_Y),
  %i(KC_A    KC_S    KC_D    KC_F    KC_G    KC_H),
  %i(KC_Z    KC_X    KC_C    KC_V    KC_B    KC_N),
  %i(KC_LSFT KC_LCTL KC_LALT KC_LGUI KC_SPC  KC_ENT)
]

# Start keyboard
kbd.start
```

## Features

- Matrix keyboard support
- Multiple layer support
- Modifier keys
- Split keyboard support
- LED indicators (NumLock, CapsLock, etc.)
- Integration with RGB, rotary encoders, joystick, mouse

## Constants

### LED Indicators

- `LED_NUMLOCK` - NumLock LED
- `LED_CAPSLOCK` - CapsLock LED
- `LED_SCROLLLOCK` - ScrollLock LED
- `LED_COMPOSE` - Compose LED
- `LED_KANA` - Kana LED

### Modifier Keys

- `KC_LCTL`, `KC_RCTL` - Left/Right Control
- `KC_LSFT`, `KC_RSFT` - Left/Right Shift
- `KC_LALT`, `KC_RALT` - Left/Right Alt
- `KC_LGUI`, `KC_RGUI` - Left/Right GUI (Windows/Command)

## Notes

- PRK Firmware is a complete keyboard firmware framework
- Supports QMK-like keycode system
- Requires appropriate hardware setup (GPIO pins for matrix)
- See PRK Firmware documentation for complete keycode reference

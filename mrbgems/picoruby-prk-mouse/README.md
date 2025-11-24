# picoruby-prk-mouse

Mouse emulation support for PRK Firmware - enables mouse control from keyboard.

## Usage

```ruby
require 'keyboard'

kbd = Keyboard.new

# Use mouse keycodes in keymap
kbd.add_layer :mouse, [
  %i(KC_MS_UP   KC_MS_DOWN KC_MS_LEFT KC_MS_RIGHT),
  %i(KC_MS_BTN1 KC_MS_BTN2 KC_MS_BTN3 KC_MS_BTN4),
  %i(KC_MS_WH_UP KC_MS_WH_DOWN KC_MS_WH_LEFT KC_MS_WH_RIGHT),
  # ...
]

kbd.start
```

## Mouse Keycodes

### Cursor Movement

- `KC_MS_UP` - Move cursor up
- `KC_MS_DOWN` - Move cursor down
- `KC_MS_LEFT` - Move cursor left
- `KC_MS_RIGHT` - Move cursor right

### Mouse Buttons

- `KC_MS_BTN1` - Left click
- `KC_MS_BTN2` - Right click
- `KC_MS_BTN3` - Middle click
- `KC_MS_BTN4` - Mouse button 4
- `KC_MS_BTN5` - Mouse button 5

### Mouse Wheel

- `KC_MS_WH_UP` - Scroll up
- `KC_MS_WH_DOWN` - Scroll down
- `KC_MS_WH_LEFT` - Scroll left
- `KC_MS_WH_RIGHT` - Scroll right

## Notes

- Keyboard appears as both keyboard and mouse to host
- Useful for keypad-style mouse control
- Can be combined with layers for efficient mouse control
- Movement speed is typically fixed (configurable in firmware)

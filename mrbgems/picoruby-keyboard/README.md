# picoruby-keyboard

Layer switching functionality for keyboard matrix in PicoRuby.

## Overview

This gem provides layer management for keyboard matrix, enabling:
- **MO (Momentary Layer)**: Activate a layer while holding a key
- **LT (Layer-Tap)**: Send a keycode on tap, activate layer on hold
- **MT (Mod-Tap)**: Send a keycode on tap, activate modifier on hold
- **TG (Toggle Layer)**: Toggle a layer on/off with each key press
- **Layer stacking**: Multiple MO keys can be pressed simultaneously
- **Transparent keys**: KC_NO falls through to lower layers

## Basic Usage

```ruby
require 'keyboard'

include USB::HID::Keycode
include LayerKeycode

# Initialize with row/col pins
kb = Keyboard.new([0, 1, 2], [3, 4, 5])

# Add default layer
kb.add_layer(:default, [
  KC_ESC,  KC_1,    MO(1),     # MO(1): Fn key
  KC_TAB,  KC_Q,    KC_W,
  KC_LSFT, KC_A,    KC_S       # KC_LSFT: modifier key (0xE1)
])

# Add function layer
kb.add_layer(:function, [
  KC_GRV,  KC_F1,   KC_NO,     # KC_NO: transparent
  KC_NO,   KC_NO,   KC_NO,
  KC_NO,   KC_NO,   KC_NO
])

# Set callback for key events
kb.start do |event|
  # Keyboard handles press/release internally
  # event[:keycode] is 0 on release automatically
  USB::HID.keyboard_send(event[:modifier], event[:keycode])
end
```

## Layer Priority

When multiple layers are active, keys are resolved in this order:
1. **Toggle locked layer** (TG activated)
2. **Momentary layer stack** (MO keys, LIFO - last pressed wins)
3. **Default layer**

If a key is `KC_NO` (transparent), it falls through to the next layer.

## Modifier Keys

Keyboard automatically handles modifier keys (Shift, Ctrl, Alt, GUI). Modifier keys use USB HID keycodes 0xE0-0xE7:

```ruby
KC_LCTL  # 0xE0 - Left Control
KC_LSFT  # 0xE1 - Left Shift
KC_LALT  # 0xE2 - Left Alt
KC_LGUI  # 0xE3 - Left GUI (Windows/Command)
KC_RCTL  # 0xE4 - Right Control
KC_RSFT  # 0xE5 - Right Shift
KC_RALT  # 0xE6 - Right Alt
KC_RGUI  # 0xE7 - Right GUI
```

### Multiple Modifier Support

Multiple modifiers can be pressed simultaneously. Keyboard automatically accumulates all active modifiers:

```ruby
# Example: Shift + Ctrl + A
# 1. Press Shift -> modifier = 0x02
# 2. Press Ctrl  -> modifier = 0x02 | 0x01 = 0x03
# 3. Press A     -> sends (modifier: 0x03, keycode: KC_A)
# 4. Release A   -> sends (modifier: 0x03, keycode: 0)
# 5. Release Ctrl -> modifier = 0x02
# 6. Release Shift -> modifier = 0x00
```

Modifier keys placed in different layers will also work correctly with the layer priority system.

## Special Keycodes

### MO(n) - Momentary Layer

Activates layer `n` while the key is held down.

```ruby
include LayerKeycode

keymap = [
  KC_A, MO(1), KC_C,  # Middle key activates layer 1 while held
  # ...
]
```

### LT(n, keycode) - Layer-Tap

Tap/hold behavior: sends `keycode` on quick tap, activates layer `n` on hold.

```ruby
include LayerKeycode

keymap = [
  KC_A, LT(1, KC_SPC), KC_C,  # Middle key: tap for space, hold for layer 1
  # ...
]
```

Behavior:
- **Tap** (quick press/release): Sends the tap keycode
- **Hold** (press for >200ms): Activates the layer
- **Hold** (press then another key): Immediately activates the layer

Configuration:
```ruby
kb = Keyboard.new([0, 1], [2, 3])
kb.tap_threshold_ms = 150  # Change tap threshold (default: 200ms)
```

### MT(modifier, keycode) - Mod-Tap

Tap/hold behavior: sends `keycode` on quick tap, activates `modifier` on hold.

```ruby
include LayerKeycode

keymap = [
  KC_A, MT(KC_LSFT, KC_ENT), KC_C,  # Middle key: tap for Enter, hold for Shift
  # ...
]
```

Supported modifiers:
- `KC_LCTL` (0xE0) - Left Control
- `KC_LSFT` (0xE1) - Left Shift
- `KC_LALT` (0xE2) - Left Alt
- `KC_LGUI` (0xE3) - Left GUI
- `KC_RCTL` (0xE4) - Right Control
- `KC_RSFT` (0xE5) - Right Shift
- `KC_RALT` (0xE6) - Right Alt
- `KC_RGUI` (0xE7) - Right GUI

Behavior:
- **Tap** (quick press/release): Sends the tap keycode
- **Hold** (press for >200ms): Activates the modifier
- **Hold** (press then another key): Immediately activates the modifier

### TG(n) - Toggle Layer

Toggles layer `n` on/off with each press.

```ruby
include LayerKeycode

keymap = [
  KC_A, TG(2), KC_C,  # Middle key toggles layer 2
  # ...
]
```

## Advanced Example

```ruby
include USB::HID::Keycode
include LayerKeycode

kb = Keyboard.new([0, 1, 2, 3], [4, 5, 6, 7], debounce_ms: 40)

# Base layer with Fn and Numpad toggle
kb.add_layer(:base, [
  KC_ESC,  KC_1,  KC_2,    KC_3,
  KC_TAB,  KC_Q,  KC_W,    KC_E,
  MO(1),   KC_A,  KC_S,    KC_D,                # MO(1): Fn key
  MT(KC_LSFT, KC_SPC), KC_Z, TG(2), KC_C        # MT: tap=Space, hold=Shift
])

# Function layer (accessed via MO(1))
kb.add_layer(:function, [
  KC_GRV,  KC_F1,  KC_F2,   KC_F3,
  KC_NO,   KC_HOME, KC_UP,   KC_END,
  KC_NO,   KC_LEFT, KC_DOWN, KC_RGHT,
  KC_NO,   KC_NO,   KC_NO,   KC_NO
])

# Numpad layer (toggled via TG(2))
kb.add_layer(:numpad, [
  KC_NO,   KC_7,   KC_8,    KC_9,
  KC_NO,   KC_4,   KC_5,    KC_6,
  KC_NO,   KC_1,   KC_2,    KC_3,
  KC_NO,   KC_0,   KC_NO,   KC_DOT
])

kb.default_layer = :base

kb.start do |event|
  USB::HID.keyboard_send(event[:modifier], event[:keycode])
end
```

## Layer Interaction Examples

### Momentary Layer
- Press MO(1): Layer 1 becomes active
- While holding MO(1): Keys from layer 1 are used
- Release MO(1): Back to default layer

### Toggle Layer
- Press TG(2): Layer 2 locks on
- Layer 2 stays active
- Press TG(2) again: Layer 2 unlocks

### Combined Usage
- TG(2) to lock numpad layer
- Press MO(1) while in numpad: Function layer takes priority
- Release MO(1): Back to numpad layer
- Press TG(2): Back to default layer

### Transparent Keys
If function layer has:
```ruby
[KC_NO, KC_NO, KC_NO, ...]  # All transparent
```
When MO(1) is pressed, all keys fall through to the default layer.

## API Reference

### Keyboard

#### `initialize(row_pins, col_pins, debounce_ms: 40)`
Create a new keyboard layer manager.
- `row_pins`: Array of GPIO pin numbers for rows
- `col_pins`: Array of GPIO pin numbers for columns
- `debounce_ms`: Debounce time in milliseconds (default: 40)

#### `add_layer(name, keymap)`
Add a layer with a name and keymap.
- `name`: Symbol for layer name
- `keymap`: Array of keycodes (size must be row_count * col_count)

#### `default_layer=(name)`
Set the default layer.
- `name`: Symbol for layer name

#### `tap_threshold_ms=(value)`
Set the tap threshold for LT/MT tap/hold keys.
- `value`: Threshold in milliseconds (default: 200)

#### `repush_threshold_ms=(value)`
Set the repush threshold for double-tap-hold detection.
- `value`: Threshold in milliseconds (default: 200)

#### `start(&block)`
Start the scanning loop (blocks forever).
- Block receives event hash: `{row:, col:, keycode:, modifier:, pressed:}`

### LayerKeycode

#### `MO(layer_index)`
Create momentary layer switch keycode.
- `layer_index`: Layer index (0-255)
- Returns: Special keycode

#### `LT(layer_index, tap_keycode)`
Create Layer-Tap keycode.
- `layer_index`: Layer index (0-15)
- `tap_keycode`: Keycode to send on tap (0-255)
- Returns: Special keycode

Tap/hold behavior: tap sends keycode, hold activates layer.

#### `MT(modifier_keycode, tap_keycode)`
Create Mod-Tap keycode.
- `modifier_keycode`: Modifier keycode (0xE0-0xE7: KC_LCTL-KC_RGUI)
- `tap_keycode`: Keycode to send on tap (0-255)
- Returns: Special keycode

Tap/hold behavior: tap sends keycode, hold activates modifier.

#### `TG(layer_index)`
Create toggle layer switch keycode.
- `layer_index`: Layer index (0-255)
- Returns: Special keycode

## Implementation Notes

- Layer indices are assigned in the order layers are added (0, 1, 2, ...)
- The first layer added becomes the default layer automatically
- MO, LT, MT, and TG keys don't generate key events themselves
- Multiple MO keys can be active simultaneously (last pressed has priority)
- Only one TG layer can be locked at a time
- KC_NO (0x00) is reserved for transparent keys
- **Modifier keys** (0xE0-0xE7) are automatically detected and converted to modifier bits
- Multiple modifiers are accumulated via bitwise OR
- On key release, `event[:keycode]` is automatically set to 0
- The `event[:pressed]` field is available for debugging or custom logic

## License

MIT

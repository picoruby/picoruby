# picoruby-keyboard-layer

Layer switching functionality for keyboard matrix in PicoRuby.

## Overview

This gem provides layer management for keyboard matrix, enabling:
- **Momentary Layer (MO)**: Activate a layer while holding a key
- **MO with Tap/Hold**: Send a keycode on tap, activate layer on hold
- **Toggle Layer (TG)**: Toggle a layer on/off with each key press
- **Layer stacking**: Multiple MO keys can be pressed simultaneously
- **Transparent keys**: KC_NO falls through to lower layers

## Installation

Add to your build configuration:

```ruby
conf.gem github: 'picoruby/picoruby', path: 'mrbgems/picoruby-keyboard-layer'
```

## Basic Usage

```ruby
require 'keyboard_layer'
require 'usb/hid'

include Keycode
include LayerKeycode

# Initialize with row/col pins
kb = KeyboardLayer.new([0, 1, 2], [3, 4, 5])

# Add default layer
kb.add_layer(:default, [
  KC_ESC,  KC_1,    KC_2,
  KC_TAB,  KC_Q,    KC_W,
  KC_LCTL, KC_A,    KC_S
])

# Add function layer with MO(1) key at position [0, 2]
kb.add_layer(:default, [
  KC_ESC,  KC_1,    MO(1),     # MO(1): Fn key
  KC_TAB,  KC_Q,    KC_W,
  KC_LCTL, KC_A,    KC_S
])

kb.add_layer(:function, [
  KC_GRV,  KC_F1,   KC_NO,     # KC_NO: transparent
  KC_NO,   KC_NO,   KC_NO,
  KC_NO,   KC_NO,   KC_NO
])

# Set callback for key events
kb.on_key_event do |event|
  if event[:pressed]
    USB::HID.keyboard_send(event[:modifier], event[:keycode])
  else
    USB::HID.keyboard_release
  end
end

# Start scanning
kb.start
```

## Layer Priority

When multiple layers are active, keys are resolved in this order:
1. **Toggle locked layer** (TG activated)
2. **Momentary layer stack** (MO keys, LIFO - last pressed wins)
3. **Default layer**

If a key is `KC_NO` (transparent), it falls through to the next layer.

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

### MO(n, keycode) - Momentary Layer with Tap/Hold

Tap/hold behavior: sends `keycode` on quick tap, activates layer `n` on hold.

```ruby
include LayerKeycode

keymap = [
  KC_A, MO(1, KC_SPC), KC_C,  # Middle key: tap for space, hold for layer 1
  # ...
]
```

Behavior:
- **Tap** (quick press/release): Sends the tap keycode
- **Hold** (press for >200ms): Activates the layer
- **Hold** (press then another key): Immediately activates the layer

Configuration:
```ruby
kb = KeyboardLayer.new([0, 1], [2, 3])
kb.tap_threshold_ms = 150  # Change tap threshold (default: 200ms)
```

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
include Keycode
include LayerKeycode

kb = KeyboardLayer.new([0, 1, 2, 3], [4, 5, 6, 7], debounce_time: 10)

# Base layer with Fn and Numpad toggle
kb.add_layer(:base, [
  KC_ESC,  KC_1,  KC_2,    KC_3,
  KC_TAB,  KC_Q,  KC_W,    KC_E,
  MO(1),   KC_A,  KC_S,    KC_D,    # MO(1): Fn key
  KC_LCTL, KC_Z,  TG(2),   KC_C     # TG(2): Numpad toggle
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

kb.on_key_event do |event|
  if event[:pressed]
    USB::HID.keyboard_send(event[:modifier], event[:keycode])
  else
    USB::HID.keyboard_release
  end
end

kb.start
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

### KeyboardLayer

#### `initialize(row_pins, col_pins, debounce_time: 5)`
Create a new keyboard layer manager.
- `row_pins`: Array of GPIO pin numbers for rows
- `col_pins`: Array of GPIO pin numbers for columns
- `debounce_time`: Debounce time in milliseconds (default: 5)

#### `add_layer(name, keymap)`
Add a layer with a name and keymap.
- `name`: Symbol for layer name
- `keymap`: Array of keycodes (size must be row_count * col_count)

#### `default_layer=(name)`
Set the default layer.
- `name`: Symbol for layer name

#### `tap_threshold_ms=(value)`
Set the tap threshold for MO tap/hold keys.
- `value`: Threshold in milliseconds (default: 200)

#### `on_key_event(&block)`
Set callback for key events.
- Block receives event hash: `{row:, col:, keycode:, modifier:, pressed:}`

#### `start`
Start the scanning loop (blocks forever).

### LayerKeycode

#### `MO(layer_index, tap_keycode = nil)`
Create momentary layer switch keycode.
- `layer_index`: Layer index (0-255 for simple MO, 0-15 for MO with tap)
- `tap_keycode`: Optional keycode to send on tap (0-255)
- Returns: Special keycode

Without `tap_keycode`: Simple momentary layer switch
With `tap_keycode`: Tap/hold behavior (tap sends keycode, hold activates layer)

#### `TG(layer_index)`
Create toggle layer switch keycode.
- `layer_index`: Layer index (0-255)
- Returns: Special keycode

## Implementation Notes

- Layer indices are assigned in the order layers are added (0, 1, 2, ...)
- The first layer added becomes the default layer automatically
- MO and TG keys don't generate key events themselves
- Multiple MO keys can be active simultaneously (last pressed has priority)
- Only one TG layer can be locked at a time
- KC_NO (0x00) is reserved for transparent keys

## License

MIT

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

kb = Keyboard.new([0, 1, 2, 3], [4, 5, 6, 7], debounce_ms: 5)

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

#### `initialize(row_pins, col_pins, debounce_ms: 5, keymap_rows: nil, keymap_cols: nil)`
Create a new keyboard layer manager.
- `row_pins`: Array of GPIO pin numbers for rows
- `col_pins`: Array of GPIO pin numbers for columns
- `debounce_ms`: Debounce time in milliseconds (default: 5, same as QMK)
- `keymap_rows`: Total rows in keymap (default: row_pins.size). For split keyboards.
- `keymap_cols`: Total columns in keymap (default: col_pins.size). For split keyboards.

#### `add_layer(name, keymap)`
Add a layer with a name and keymap.
- `name`: Symbol for layer name
- `keymap`: Array of keycodes (size must be keymap_rows * keymap_cols)

#### `default_layer=(name)`
Set the default layer.
- `name`: Symbol for layer name

#### `tap_threshold_ms=(value)`
Set the tap threshold for LT/MT tap/hold keys.
- `value`: Threshold in milliseconds (default: 200)

#### `repush_threshold_ms=(value)`
Set the repush threshold for double-tap-hold detection.
- `value`: Threshold in milliseconds (default: 200)

#### `inject_event(row, col, pressed)`
Inject an external key event (for split keyboard support).
- `row`: Row index in keymap coordinates
- `col`: Column index in keymap coordinates
- `pressed`: true for press, false for release

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

## Combos

Combos allow you to trigger a specific action when multiple keys are pressed simultaneously. This is a QMK-compatible feature.

### Basic Usage

```ruby
include USB::HID::Keycode
include LayerKeycode

kb = Keyboard.new([0, 1], [2, 3])

# Define combo: A+B pressed together -> sends ESC
kb.add_combo([KC_A, KC_B], KC_ESC)

# Define combo: C+D pressed together -> sends TAB
kb.add_combo([KC_C, KC_D], KC_TAB)

# Define combo with 3 keys: A+B+C -> sends ENTER
kb.add_combo([KC_A, KC_B, KC_C], KC_ENT)
```

### Configuration

```ruby
# Set combo detection window (default: 50ms)
# Keys must be pressed within this time window to trigger combo
kb.combo_term_ms = 50

# Optional: Force combos to check from specific layer (default: current layer)
kb.combo_reference_layer = :default
```

### How Combos Work

1. **Key Press Detection**: When a key that is part of any combo is pressed, it goes into a combo buffer
2. **Combo Matching**: The system checks if all keys in any defined combo are present in the buffer
3. **Longest Match Wins**: If multiple combos match (e.g., A+B and A+B+C), the longer combo takes priority
4. **Timeout**: If combo doesn't complete within `combo_term_ms`, buffered keys are sent as normal key presses
5. **Early Release**: If a combo key is released before combo triggers, it sends a tap of that key

### Layer Interaction

- By default, combos check keycodes from the **current active layer**
- Use `combo_reference_layer=` to force combo detection from a specific layer

```ruby
# Layer setup
kb.add_layer(:default, [KC_A, KC_B, KC_C, KC_D])
kb.add_layer(:layer1, [KC_1, KC_2, KC_3, KC_4])

# Combo defined with KC_A and KC_B
kb.add_combo([KC_A, KC_B], KC_ESC)

# In :default layer: Press physical keys for A+B -> ESC triggers
# In :layer1 layer: A+B keycodes don't exist on current layer, combo won't trigger
```

### Combo vs LT/MT Interaction

- Combo detection takes priority over normal key processing
- LT/MT keys are processed before combo detection, so their base keycodes can be used in combos
- Combo buffer is cleared when combo triggers, preventing interference

### Example: Gaming Combos

```ruby
include USB::HID::Keycode

kb = Keyboard.new([0, 1, 2], [3, 4, 5])

kb.add_layer(:default, [
  KC_W, KC_A, KC_S,
  KC_D, KC_SPC, KC_LSFT
])

# Gaming combos
kb.add_combo([KC_W, KC_A], KC_Q)      # Diagonal movement -> Q ability
kb.add_combo([KC_W, KC_D], KC_E)      # Diagonal movement -> E ability
kb.add_combo([KC_SPC, KC_LSFT], KC_R) # Jump + Run -> Ultimate

kb.combo_term_ms = 30  # Faster detection for gaming

kb.start do |event|
  USB::HID.keyboard_send(event[:modifier], event[:keycode])
end
```

## Split Keyboard Support

This gem supports split keyboards where one side (master) holds the entire keymap
and the other side (slave) sends key events via UART or other communication protocols.

### Architecture

- **Master**: Holds the entire keymap and runs `Keyboard` class
- **Slave**: Uses `KeyboardMatrix` directly and sends events to master
- **User**: Implements the communication protocol (UART, I2C, etc.)

### Keymap Size Configuration

For split keyboards, the keymap can be larger than the local matrix:

```ruby
# Symmetric split: left 2x3, right 2x3
# Master matrix is 2x3, but keymap covers both sides (2x6)
kb = Keyboard.new([0, 1], [2, 3, 4], keymap_cols: 6)

# Asymmetric split: left 2x3, right 3x4
# Keymap needs 3 rows x 7 cols to cover both sides
kb = Keyboard.new([0, 1], [2, 3, 4], keymap_rows: 3, keymap_cols: 7)
```

### Event Injection

Use `inject_event` to push slave key events to the master:

```ruby
# Inject slave key event (row, col, pressed)
kb.inject_event(0, 3, true)   # Slave row 0, col 3, pressed
kb.inject_event(0, 3, false)  # Slave row 0, col 3, released
```

### Example: Symmetric Split Keyboard

```ruby
# === Master side ===
require 'keyboard'
require 'uart'

include USB::HID::Keycode
include LayerKeycode

# Master: 2x3 matrix, full keymap is 2x6
kb = Keyboard.new([0, 1], [2, 3, 4], keymap_cols: 6)

kb.add_layer(:default, [
  # Left (master cols 0-2)    Right (slave cols 3-5)
  KC_A, KC_B, KC_C,           KC_D, KC_E, KC_F,
  KC_1, KC_2, KC_3,           KC_4, KC_5, KC_6
])

# User handles UART protocol
uart = UART.new(unit: 0, txd: 8, rxd: 9)

def parse_slave_data(data)
  # User-defined protocol parsing
  # Returns [row, col, pressed]
end

kb.start do |event|
  # Check for slave data
  if data = uart.read_nonblock
    row, col, pressed = parse_slave_data(data)
    kb.inject_event(row, col + 3, pressed)  # Offset col by 3 for right side
  end
  USB::HID.keyboard_send(event[:modifier], event[:keycode])
end
```

```ruby
# === Slave side ===
require 'keyboard_matrix'
require 'uart'

def encode_event(event)
  # User-defined protocol encoding
end

matrix = KeyboardMatrix.new([0, 1], [2, 3, 4])
uart = UART.new(unit: 0, txd: 9, rxd: 8)

loop do
  if event = matrix.scan
    uart.write(encode_event(event))
  end
  sleep_ms(1)
end
```

### Example: Asymmetric Split Keyboard

```ruby
# Master: 2x3 matrix, slave: 3x4 matrix
# Full keymap: 3 rows x 7 cols
kb = Keyboard.new([0, 1], [2, 3, 4], keymap_rows: 3, keymap_cols: 7)

kb.add_layer(:default, [
  # Cols: 0   1   2        3   4   5   6
  KC_A, KC_B, KC_C,    KC_D, KC_E, KC_F, KC_G,    # Row 0 (master uses 0-2)
  KC_1, KC_2, KC_3,    KC_4, KC_5, KC_6, KC_7,    # Row 1 (master uses 0-2)
  KC_NO, KC_NO, KC_NO, KC_8, KC_9, KC_0, KC_MINS  # Row 2 (slave only)
])

kb.start do |event|
  if data = uart.read_nonblock
    row, col, pressed = parse_slave_data(data)
    kb.inject_event(row, col + 3, pressed)  # Offset col by 3
  end
  USB::HID.keyboard_send(event[:modifier], event[:keycode])
end
```

### Design Notes

- Master's columns should be at the beginning of the keymap (cols 0, 1, 2...)
- Slave events are offset when injected via `inject_event`
- Use `KC_NO` for keymap positions not physically present on master
- All features (LT, MT, MO, TG, combos) work with split keys

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

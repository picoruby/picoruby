# picoruby-keyboard-matrix

GPIO keyboard matrix scanner

## Features

- Row/column GPIO matrix scanning
- Software debouncing
- N-key rollover support
- Event-driven callback system
- Configurable debounce time

## Usage

```ruby
include Keycode

# Define GPIO pins
row_pins = [0, 1, 2, 3, 4, 5]  # 6 rows
col_pins = [6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17]  # 12 columns

# Define keymap (row-major order: 6 rows × 12 cols = 72 keys)
keymap = [
  # Row 0
  KC_ESC, KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9, KC_0, KC_MINUS,
  # Row 1
  KC_TAB, KC_Q, KC_W, KC_E, KC_R, KC_T, KC_Y, KC_U, KC_I, KC_O, KC_P, KC_BSLASH,
  # Row 2
  KC_LCTL, KC_A, KC_S, KC_D, KC_F, KC_G, KC_H, KC_J, KC_K, KC_L, KC_SCOLON, KC_QUOTE,
  # Row 3
  KC_LSFT, KC_Z, KC_X, KC_C, KC_V, KC_B, KC_N, KC_M, KC_COMMA, KC_DOT, KC_SLASH, KC_ENTER,
  # Row 4
  KC_LGUI, KC_LALT, KC_SPACE, KC_SPACE, KC_SPACE, KC_SPACE, KC_SPACE, KC_RALT, KC_RGUI, KC_LEFT, KC_DOWN, KC_UP,
  # Row 5
  KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F7, KC_F8, KC_F9, KC_F10, KC_F11, KC_F12
]

# Optional: Define modifier map (0 for most keys)
modifier_map = Array.new(72, 0)

# Create keyboard matrix instance
matrix = KeyboardMatrix.new(row_pins, col_pins, keymap, modifier_map)

# Set debounce time (optional, default is 5ms)
matrix.debounce_time = 10

# Method 1: Polling mode
loop do
  event = matrix.scan
  if event
    if event[:pressed]
      USB::HID.keyboard_send(event[:modifier], event[:keycode])
    else
      USB::HID.keyboard_release
    end
  end
  sleep_ms(1)
end

# Method 2: Callback mode
matrix.start do |event|
  if event[:pressed]
    USB::HID.keyboard_send(event[:modifier], event[:keycode])
  else
    USB::HID.keyboard_release
  end
end
```

## Event Structure

```ruby
{
  row: 0,           # Row index (0-based)
  col: 1,           # Column index (0-based)
  keycode: 0x04,    # HID keycode (from keymap)
  modifier: 0x00,   # HID modifier bitmap (from modifier_map)
  pressed: true     # true = pressed, false = released
}
```

## Hardware Connection

### Row-Column Diode Matrix

```
         Col0  Col1  Col2  ...
          |     |     |
Row0 ----+-----+-----+---- (with diodes)
          |     |     |
Row1 ----+-----+-----+----
          |     |     |
Row2 ----+-----+-----+----
```

- Rows: GPIO outputs (driven low during scan)
- Columns: GPIO inputs with pull-up resistors
- Each key has a diode (cathode to row, anode to column)

## Debouncing

Software debouncing is implemented to filter out key bounce noise. Default is 5ms, configurable via `debounce_time=`.

## License

MIT

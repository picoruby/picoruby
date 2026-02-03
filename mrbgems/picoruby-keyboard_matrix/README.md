# picoruby-keyboard_matrix

GPIO keyboard matrix scanner

## Features

- Row/column GPIO matrix scanning
- Software debouncing
- N-key rollover support
- Event-driven callback system
- Configurable debounce time
- **Raw matrix events only** - no keymap processing

## Usage

**Note:** This gem only scans the physical matrix and reports raw row/col events. For keymap and layer management, use `picoruby-keyboard_layer`.

```ruby
# Define GPIO pins
row_pins = [0, 1, 2, 3]
col_pins = [4, 5, 6, 7]

# Create keyboard matrix instance (no keymap needed)
matrix = KeyboardMatrix.new(row_pins, col_pins)

# Set debounce time (optional, default is 40ms)
matrix.debounce_ms = 20

# Method 1: Polling mode
loop do
  event = matrix.scan
  if event
    puts "Key at [#{event[:row]}, #{event[:col]}] #{event[:pressed] ? 'pressed' : 'released'}"
  end
  sleep_ms(1)
end

# Method 2: Callback mode
matrix.start do |event|
  row = event[:row]
  col = event[:col]
  pressed = event[:pressed]
  # Handle raw matrix event
end
```

## Event Structure

```ruby
{
  row: 0,           # Row index (0-based)
  col: 1,           # Column index (0-based)
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

Software debouncing is implemented to filter out key bounce noise. Default is 5ms, configurable via `debounce_ms=`.

## License

MIT

# picoruby-rmt

RMT (Remote Control Transceiver) library for PicoRuby - used for precise timing control of GPIO signals.

## Usage

```ruby
require 'rmt'

# Initialize RMT for WS2812 LED (NeoPixel)
rmt = RMT.new(
  25,          # GPIO pin
  t0h_ns: 350, # Time for bit 0, high period (ns)
  t0l_ns: 800, # Time for bit 0, low period (ns)
  t1h_ns: 700, # Time for bit 1, high period (ns)
  t1l_ns: 600, # Time for bit 1, low period (ns)
  reset_ns: 50000  # Reset period (ns)
)

# Write data (RGB values for LED)
rmt.write([255, 0, 0])  # Red
rmt.write([0, 255, 0])  # Green
rmt.write([0, 0, 255])  # Blue
```

## API

### Methods

- `RMT.new(pin, t0h_ns:, t0l_ns:, t1h_ns:, t1l_ns:, reset_ns:)` - Initialize RMT on GPIO pin
  - `pin`: GPIO pin number
  - `t0h_ns`: High time for bit 0 in nanoseconds
  - `t0l_ns`: Low time for bit 0 in nanoseconds
  - `t1h_ns`: High time for bit 1 in nanoseconds
  - `t1l_ns`: Low time for bit 1 in nanoseconds
  - `reset_ns`: Reset/pause time in nanoseconds
- `write(*data)` - Write data with precise timing

## Notes

- RMT is commonly used for controlling addressable LEDs (WS2812, SK6812, etc.)
- Timing parameters must match the requirements of the target device
- Can also be used for other protocols requiring precise timing (IR, etc.)

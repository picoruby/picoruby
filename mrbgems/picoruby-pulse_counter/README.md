# picoruby-pulse_counter

PulseCounter library for PicoRuby - counts quadrature encoder pulses in hardware.

The A/B phases are decoded with x4 resolution (every rising and falling edge of both phases), so the CPU only has to read the count. The hardware counter is 16-bit, but overflows are accumulated by the driver and `count` keeps growing beyond that range.

## Usage

```ruby
require 'pulse_counter'

encoder = PulseCounter.new(5, 6)   # pin_a, pin_b

loop do
  puts encoder.count       # signed; the sign indicates the direction
  sleep_ms 100
end
```

## API

- `PulseCounter.new(pin_a, pin_b, glitch_ns: 1000, pull_up: true)` - Start counting on the given pins
  - `pin_a`, `pin_b`: GPIO pins connected to the A and B phases
  - `glitch_ns`: Pulses shorter than this are ignored (0 disables the filter)
  - `pull_up`: Enable the internal pull-up resistors on both pins
- `count` - Accumulated count (increases in one direction, decreases in the other)
- `clear` - Reset the count to zero

## Notes

- Supported on ESP32 series with a PCNT (Pulse Counter) peripheral (not available on ESP32-C2/C3).
- The number of encoders is limited by the number of PCNT units on the chip (4 on ESP32-S3).
- Swap `pin_a` and `pin_b` to reverse the sign of the count.

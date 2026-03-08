# picoruby-rotary_encoder

Pure Ruby rotary encoder driver for PicoRuby, built on top of
[picoruby-irq](../picoruby-irq/README.md).

Unlike [picoruby-prk-rotary_encoder](../picoruby-prk-rotary_encoder/README.md),
which uses a C-level GPIO IRQ callback, this gem implements the quadrature
decoding state machine entirely in Ruby using the high-level `IRQ` API.

## Hardware

Connect a standard incremental rotary encoder (two-phase quadrature output)
to two GPIO pins.  Both pins are configured as inputs with internal pull-ups,
so the encoder contacts should pull the lines to GND when active.

```
MCU                     Encoder
------                  -------
GPIO pin_a  ----------- A (phase A)
GPIO pin_b  ----------- B (phase B)
GND         ----------- C (common)
3.3V (optional) ------- (external pull-up not required)
```

## Usage

```ruby
require 'rotary_encoder'

enc = RotaryEncoder.new(2, 3)   # pin_a = GP2, pin_b = GP3

enc.clockwise do
  puts "CW"
end

enc.counterclockwise do
  puts "CCW"
end

loop do
  IRQ.process   # dispatches GPIO edge events; cw/ccw callbacks fire here
  sleep_ms(1)
end
```

## API

### `RotaryEncoder.new(pin_a, pin_b)` -> `RotaryEncoder`

Creates a new encoder instance and immediately registers IRQ handlers for
both GPIO pins (EDGE_FALL | EDGE_RISE).

| Parameter | Type    | Description           |
|-----------|---------|---------------------- |
| `pin_a`   | Integer | GPIO pin number for A |
| `pin_b`   | Integer | GPIO pin number for B |

### `#clockwise { } -> void` (alias: `cw`)

Sets the block to call when a clockwise rotation is detected.

### `#counterclockwise { } -> void` (alias: `ccw`)

Sets the block to call when a counterclockwise rotation is detected.

## Quadrature Decoding

The state machine is a direct port of the C implementation in
`picoruby-prk-rotary_encoder`.  Both phase-A and phase-B edges are captured
via IRQ.  The active-low pin state is encoded as:

```
current = (pin_a LOW ? 0b10 : 0b00) | (pin_b LOW ? 0b01 : 0b00)
```

A 6-bit status register tracks `[rotation_dir(2)][prev(2)][current(2)]`:

- When transitioning away from rest (`0b00`), the first edge determines the
  candidate direction (`0b01` = CCW candidate, `0b10` = CW candidate).
- When both pins return to rest, the candidate direction and the preceding
  state confirm the actual rotation direction.

## Notes

- `IRQ.process` must be called in the main loop. The cw/ccw callbacks fire
  directly inside `IRQ.process` when a complete quadrature cycle is detected.
- No debounce is applied at the IRQ layer; the state machine inherently
  filters spurious edges by requiring the full quadrature cycle.
- Multiple encoder instances are supported (up to the platform IRQ limit).

# picoruby-pio

PIO (Programmable I/O) library for RP2040/RP2350.

Write PIO assembly programs in Ruby DSL, load them into state machines,
and control FIFO I/O -- all from Ruby.

## Assembler DSL

`PIO.asm` takes a block and returns a `PIO::Program`.
All PIO instructions are available as methods inside the block.

```ruby
prog = PIO.asm(side_set: 1) do
  wrap_target
  label :bitloop
  out :x, 1,                        side: 0, delay: 2
  jmp :do_zero, cond: :not_x,  side: 1, delay: 1
  label :do_one
  jmp :bitloop,                      side: 1, delay: 4
  label :do_zero
  nop                                side: 0, delay: 4
  wrap
end
```

### Instructions

| Method | PIO instruction | Notes |
|--------|----------------|-------|
| `jmp addr, cond:, side:, delay:` | JMP | Conditions: `:always`, `:not_x`, `:x_dec`, `:not_y`, `:y_dec`, `:x_ne_y`, `:pin`, `:not_osre` |
| `wait polarity, source, index` | WAIT | Sources: `:gpio`, `:pin`, `:irq` |
| `in_ source, bit_count` | IN | `in` is a Ruby reserved word |
| `out dest, bit_count` | OUT | |
| `push iffull:, block:` | PUSH | |
| `pull ifempty:, block:` | PULL | |
| `mov dest, source, op:` | MOV | Ops: `:none`, `:invert`, `:reverse` |
| `irq index, wait:, clear:, relative:` | IRQ | |
| `set dest, value` | SET | |
| `nop` | NOP | Alias for `mov :y, :y` |

All instructions accept optional `side:` and `delay:` keyword arguments.

### Sources / Destinations

Specified as symbols: `:pins`, `:x`, `:y`, `:null`, `:isr`, `:osr`,
`:pindirs`, `:pc`, `:exec`, `:status`.

### Labels

```ruby
label :my_label      # define
jmp :my_label        # reference (forward references work)
```

### Wrap

```ruby
wrap_target          # marks loop start
# ...instructions...
wrap                 # marks loop end (last instruction before wrap call)
```

## StateMachine

```ruby
sm = PIO::StateMachine.new(
  pio: PIO::PIO0,
  sm: 0,
  program: prog,
  freq: 8_000_000,
  out_pins: 2,
  out_pin_count: 1,
  set_pins: -1,            # -1 = unused
  set_pin_count: 1,
  in_pins: -1,
  sideset_pins: 2,
  jmp_pin: -1,
  out_shift_right: false,
  out_shift_autopull: true,
  out_shift_threshold: 24,
  in_shift_right: true,
  in_shift_autopush: false,
  in_shift_threshold: 32,
  fifo_join: PIO::FIFO_JOIN_TX
)
```

### Control

```ruby
sm.start              # enable state machine
sm.stop               # disable state machine
sm.restart            # clear internal state, keep program
sm.exec(instruction)  # execute a single instruction immediately
```

### FIFO

```ruby
sm.put_buffer([0xFF0000, 0x00FF00])  # blocking write (array of 32-bit words)
sm.put_nonblocking(val)              # => true / false

sm.get                    # blocking read
sm.get_nonblocking        # => Integer or nil

sm.tx_level               # => 0..4 (0..8 if joined)
sm.rx_level               # => 0..4 (0..8 if joined)
sm.tx_full?               # => bool
sm.tx_empty?              # => bool
sm.rx_full?               # => bool
sm.rx_empty?              # => bool
sm.clear_fifos
sm.drain_tx               # wait until TX FIFO is empty

sm.put_bytes("Hello")             # send each byte as a 32-bit word
sm.put_bytes([0xFF, 0x00, 0xAB])  # same, from Array
```

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `PIO::PIO0` | 0 | PIO block 0 |
| `PIO::PIO1` | 1 | PIO block 1 |
| `PIO::FIFO_JOIN_NONE` | 0 | Separate TX/RX FIFOs (4 entries each) |
| `PIO::FIFO_JOIN_TX` | 1 | Join both FIFOs for TX (8 entries) |
| `PIO::FIFO_JOIN_RX` | 2 | Join both FIFOs for RX (8 entries) |

## Examples

### NeoPixel (WS2812)

```ruby
ws2812 = PIO.asm(side_set: 1) do
  wrap_target
  label :bitloop
  out :x, 1,                        side: 0, delay: 2
  jmp :do_zero, cond: :not_x,  side: 1, delay: 1
  label :do_one
  jmp :bitloop,                      side: 1, delay: 4
  label :do_zero
  nop                                side: 0, delay: 4
  wrap
end

sm = PIO::StateMachine.new(
  pio: PIO::PIO0, sm: 0,
  program: ws2812, freq: 8_000_000,
  sideset_pins: 16,
  out_shift_right: false,
  out_shift_autopull: true,
  out_shift_threshold: 24,
  fifo_join: PIO::FIFO_JOIN_TX
)
sm.start

# Send GRB data for 3 LEDs (left-align 24-bit data)
pixels = [0x00FF00 << 8, 0xFF0000 << 8, 0x0000FF << 8]  # Green, Red, Blue
sm.put_buffer(pixels)
```

### UART TX

```ruby
uart_tx = PIO.asm(side_set: 1, side_set_optional: true) do
  pull                  side: 1            # stall with TX high
  set :x, 7,           side: 0, delay: 7  # start bit + counter
  label :bitloop
  out :pins, 1                             # shift 1 bit
  jmp :bitloop, cond: :x_dec, delay: 6
  wrap
end

sm = PIO::StateMachine.new(
  pio: PIO::PIO0, sm: 1,
  program: uart_tx,
  freq: 9600 * 8,
  out_pins: 0,
  sideset_pins: 0,
  out_shift_right: true,
  fifo_join: PIO::FIFO_JOIN_TX
)
sm.start

sm.put_bytes("Hello")
```

## Architecture

```
mrblib/pio.rb                 PIO module, Assembler DSL, Program class
mrblib/pio_state_machine.rb   PIO::StateMachine (Ruby layer)
include/pio.h                 Platform-agnostic C header
src/mruby/pio.c               mruby C bindings
src/pio.c                     VM dispatcher
ports/rp2040/pio.c            Pico SDK implementation
sig/pio.rbs                   RBS type signatures
```

Instruction encoding is done entirely in Ruby (`PIO::Assembler`),
so it can be tested on the host without hardware.
The C layer handles only hardware interaction:
program loading, state machine configuration, and FIFO operations.

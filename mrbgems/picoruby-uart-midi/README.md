# picoruby-uart-midi

MIDI 1.0 input and output over PicoRuby's UART driver.

```ruby
require "uart-midi"

midi = UART::MIDI.new(
  unit: :PICORB_UART_RP2040_UART0,
  txd_pin: 0,
  rxd_pin: 1,
  time_signature: [4, 4]
)

loop do
  event = midi.getevent
  p event
  p [midi.bpm, midi.bar, midi.beat, midi.tick]
end
```

`getevent` blocks until a complete event is available. `putevent` accepts the
same command and values used by received event arrays.

# picoruby-uart-midi

MIDI 1.0 input and output over PicoRuby's UART driver.

`UART::MIDI` is a MIDIBASE transport. It provides blocking `getevent`,
wire-format `putevent`, clock helpers, and `handle(event, **context)` for use as
a `MIDIBASE::Router` sink.

## Basic input

```ruby
require "uart-midi"

midi_keyboard = UART::MIDI.new(
  unit: :RP2040_UART1,
  txd_pin: 4,
  rxd_pin: 5,
  time_signature: [4, 4]
)

loop do
  event = midi_keyboard.getevent
  case event[0]
  when :note_on
    p [:note_on, event[1], event[2], event[3]]   # channel, note, velocity
  when :note_off
    p [:note_off, event[1], event[2], event[3]]  # channel, note, velocity
  when :timing_clock
    # ignore
  else
    p event
  end
end
```

`getevent` blocks until a complete event is available. `putevent` accepts the
same command and values used by received event arrays.

## Router input

Route UART input explicitly when sharing events with synthesizers, MIDI Clock
consumers, or other sinks:

```ruby
require "uart-midi"

midi = UART::MIDI.new(
  unit: :RP2040_UART1,
  txd_pin: 4,
  rxd_pin: 5
)

router = MIDIBASE::Router.new
router.connect(:uart, synth, priority: 100, only: MIDIBASE::CHANNEL_EVENTS)
router.connect(:uart, clock, only: MIDIBASE::TRANSPORT_EVENTS)

loop do
  router.emit(:uart, midi.getevent)
end
```

## Router output

`UART::MIDI#handle` forwards received event arrays to `putevent`, so a UART
MIDI port can be connected as an output sink for another Router source:

```ruby
router.connect(:mml, midi, only: MIDIBASE::WIRE_EVENTS)
```

The Router does not create an implicit MIDI Thru connection. Connect every
source and sink that should exchange events.

# picoruby-midibase-mml

MIDI-oriented Music Macro Language parser and sequencer for PicoRuby.

The library emits MIDI event arrays instead of driving a sound generator. Sequence parsing is incremental and uses a pull API, so a complete song is not expanded into an in-memory event list.

## Autonomous playback

```ruby
sequence = MIDIBASE::MML::Sequence.new(tracks, loop: true)
player = MIDIBASE::MML::Player.new(
  sequence,
  clock: MIDIBASE::MML::InternalClock.new,
  output: router.output(:mml)
)
player.start
```

## External MIDI Clock

```ruby
clock = MIDIBASE::MML::MIDIClock.new
player = MIDIBASE::MML::Player.new(sequence, clock: clock, output: router.output(:mml))
router.connect(:uart, clock, only: MIDIBASE::TRANSPORT_EVENTS)
player.start

while true
  router.emit(:uart, midi.getevent)
end
```

The same UART input can control the score clock and play the local synth at the same time. Give live input a higher route priority so it may steal score voices, while score events cannot steal voices currently used by the controller:

```ruby
router.connect(:mml, synth, priority: 0)
router.connect(:uart, synth, priority: 100, only: MIDIBASE::CHANNEL_EVENTS)
router.connect(:uart, clock, only: MIDIBASE::TRANSPORT_EVENTS)
```

To play an external MIDI sound module instead, connect the MML source to a MIDI output sink. `UART::MIDI#handle` forwards an event to `#putevent`:

```ruby
router.connect(:mml, midi, only: MIDIBASE::WIRE_EVENTS)
```

MIDI Clock controls both tempo and transport. START rewinds, CONTINUE resumes, and STOP pauses score progress. Timing between the 24 clocks per quarter note is interpolated from the measured pulse interval. If the clock disappears, playback does not advance beyond the next expected pulse boundary.

`MIDIBASE::MML::Sequence#next_event` returns `[delta_ticks, event]` at 480 PPQN. Standard commands are emitted as MIDI events. PSG-only envelope, mixer, noise, and LFO controls use `[:psg, channel, command, *values]` extension events.

## Named drums

Prefix a drum name with `!` to emit General MIDI percussion events on MIDI channel 10. Drum hits use the same default length, explicit length, dots, and `q` gate as regular notes:

```ruby
sequence = MIDIBASE::MML::Sequence.new([
  "t120 l8 !kick !snare !ch !snare !kick4."
])
```

Full names and short aliases are available:

| Full name      | Alias | MIDI note |
|----------------|-------|----------:|
| `kick`         | `bd`  | 35        |
| `snare`        | `sd`  | 38        |
| `closed_hihat` | `ch`  | 42        |
| `open_hihat`   | `oh`  | 46        |
| `low_tom`      | `lt`  | 41        |
| `mid_tom`      | `mt`  | 45        |
| `high_tom`     | `ht`  | 48        |

Names are case-insensitive. Because these are standard MIDI note events, the same MML works with `PSG::Synth` and external MIDI sound modules.

This gem supports PicoRuby's mruby VM. FemtoRuby/mruby-c is not supported.

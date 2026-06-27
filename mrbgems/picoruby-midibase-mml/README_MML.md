# MML command and playback reference

`picoruby-midibase-mml` parses Music Macro Language (MML) tracks and emits
MIDI-compatible events. Parsing is incremental: `MIDIBASE::MML::Sequence`
returns one event at a time, and `MIDIBASE::MML::Player` schedules those events
using either its internal clock or an external MIDI Clock.

The PSG-specific commands for envelopes, vibrato, mixer, and noise are emitted
as MIDIBASE extension events. They are understood by `PSG::Synth` from the
`picoruby-psg` gem and can be ignored by other outputs.

## Playing MML with PSG

The following example uses an MCP4922 DAC, two melodic tracks, and a drum
track. `!snare` and the other named drum tokens emit General MIDI percussion
notes on MIDI channel 10. `PSG::Synth` converts those notes to its built-in drum
sounds.

```ruby
require "psg"
require "midibase-mml"

tracks = [
  "t130 @0 s0 m1200 o5 l1 [e- | c | e | b-]4",
  "t130 @0 s0 m1200 o3 l1 [f  | d | f | c ]4",
  "t130 l8 [!bd !ch !snare !ch !bd !oh !snare !bd]16"
]

driver = PSG::Driver.new(:mcp4922, copi: 15, sck: 14, cs: 13, ldac: 12)
synth = PSG::Synth.new(driver).start
router = MIDIBASE::Router.new
router.connect(:mml, synth, priority: 0)

sequence = MIDIBASE::MML::Sequence.new(tracks)
player = MIDIBASE::MML::Player.new(
  sequence,
  output: router.output(:mml)
).start

player.join
synth.stop.join
driver.join
```

For the complete runnable version, see
[`picoruby-psg/example/mml_two_voices_drums_mcp4922.rb`](../picoruby-psg/example/mml_two_voices_drums_mcp4922.rb).

`MIDIBASE::MML::Player#start` starts playback in a task and returns the player.
The player also provides `pause`, `resume`, `rewind`, `stop`, and `join`.
See [README.md](README.md) for external MIDI Clock and routing examples.

## Supported MML commands

Commands and drum names are case-insensitive. Whitespace is ignored.

| Command      | Description                                                                             |
|--------------|-----------------------------------------------------------------------------------------|
| `a`..`g`     | Note. Append `+` or `#` for a sharp and `-` for a flat, for example `c+`, `a#`, or `d-` |
| `r`          | Rest                                                                                    |
| `tN`         | Tempo in BPM, for example `t120`                                                        |
| `oN`         | Octave (`1`..`8`), for example `o4`                                                     |
| `lN`         | Default note length, for example `l4` for a quarter note or `l8` for an eighth note     |
| `qN`         | Gate time (`1`..`8`); `q8` uses the full note length and `q4` uses half                 |
| `<`, `>`     | Decrease or increase the octave by one                                                  |
| `pN`         | Pan (`0`..`15`): `p1` is left, `p8` is center, and `p15` is right                       |
| `vN`         | Volume (`0`..`15`); also switches a PSG channel from envelope to fixed volume           |
| `@N`         | MIDI program (`0`..`127`); with `PSG::Synth`, `@0`..`@3` select square, triangle, sawtooth, and inverse sawtooth |
| `k+N`, `k-N` | Transpose subsequent notes up or down by `N` semitones                                  |
| `zN`         | Detune downward (`0`..`128`); `z128` lowers the pitch by one octave                     |
| `jD,R`       | Vibrato: depth `D` in semitones and rate `R` in 0.1 Hz units                            |
| `sN`         | PSG envelope shape (`0`..`15`); enables envelope volume for the channel                 |
| `mN`         | PSG envelope period (`0`..`65535`)                                                      |
| `{ ... }`    | Enable PSG legato inside the braces so note changes do not reset the envelope           |
| `xN`         | PSG mixer: `x0` is tone, `x1` is noise, and `x2` is tone plus noise                     |
| `yN`         | PSG noise period (`0`..`31`)                                                            |
| `!name`      | Named drum; accepts the same length, dots, gate, and repeat syntax as a note            |
| `.`          | Dotted length; one dot is 1.5 times, two dots are 1.75 times, and up to three dots are recognized |
| `[ ... ]N`   | Repeat the enclosed block `N` times; nesting is supported up to eight levels            |
| `$`          | Segno (continuous-loop start position); used when the sequence has `loop: true`         |
| `\|`         | Barline metadata; does not consume time                                                 |

For example:

```text
[cdef]2       => cdefcdef
[a[bc]2]3     => abcbcabcbcabcbc
[g4.|f4.]2    => g4.f4.g4.f4.
```

`loop: true` enables continuous playback from `$` when the end of a track is
reached. It is separate from the finite `[ ... ]N` repeat syntax:

```ruby
sequence = MIDIBASE::MML::Sequence.new(["t120 $ c d e f"], loop: true)
```

## Named drums

A drum token starts with `!` followed by a full name or alias:

| Full name      | Alias | MIDI note |
|----------------|-------|----------:|
| `kick`         | `bd`  | 35        |
| `snare`        | `sd`  | 38        |
| `closed_hihat` | `ch`  | 42        |
| `open_hihat`   | `oh`  | 46        |
| `low_tom`      | `lt`  | 41        |
| `mid_tom`      | `mt`  | 45        |
| `high_tom`     | `ht`  | 48        |

Lengths work exactly as they do for melodic notes, so this is valid:

```text
t120 l8 q6 [!kick !ch !snare !ch]3 !kick4.
```

All named drums are emitted as standard `note_on` and `note_off` events on
zero-based channel `9`, which is MIDI channel 10. Consequently the same MML can
drive `PSG::Synth` or an external General MIDI sound module. For PSG playback,
the actual sounds are selected by `PSG.assign_drum_sound`; see the
`picoruby-psg` README for its default mappings and customization API.

## Sequence API and event format

Create a sequence from one to sixteen tracks:

```ruby
sequence = MIDIBASE::MML::Sequence.new(
  tracks,
  channels: [0, 1, 2], # optional; defaults to each track's zero-based index
  loop: false,
  ppqn: 480,
  time_signature: [4, 4],
  exception: true
)
```

`Sequence#next_event` merges all tracks and returns `[delta_ticks, event]`, or
`nil` at the end. `delta_ticks` uses the sequence's PPQN; it is not a
millisecond value. `Sequence#reset` rewinds all tracks.

Standard MML commands produce MIDIBASE events such as:

```ruby
[:note_on, channel, note, velocity]
[:note_off, channel, note, velocity]
[:control_change, channel, controller, value]
[:program_change, channel, program]
[:pitch_bend, channel, value]
[:tempo, bpm]
[:barline, bar_number]
[:loop_point]
```

PSG-only commands produce extension events:

```ruby
[:psg, channel, :env_period, period]
[:psg, channel, :env_shape, shape]
[:psg, channel, :lfo, depth_in_cents, rate]
[:psg, channel, :mixer, mode]
[:psg, channel, :noise, period]
```

`MIDIBASE::MML::Player` consumes these delta ticks, handles tempo events, and
forwards the remaining events to the object supplied as `output:`.

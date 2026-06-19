# MML — Music Macro Language Parser for PicoRuby

This library parses and compiles Music Macro Language (MML) strings into timed events for PSG (Programmable Sound Generator) playback.
It supports multi-track sequencing, tempo and envelope control, vibrato (LFO), panning, volume adjustment, and loop structures.

## 🛠 Design Highlights

- 🎶 Supports envelope modulation and vibrato per channel
- 🔁 Nested loop expansion with safe recursion
- 📡 Delta-time event emission enables precise scheduling
- 🧩 Compatible with external PSG::Driver implementations (see picoruby-psg)

## ✅ Basic Usage

```ruby
tracks = [
  '@0 T120 S0 M800 L4 O5 f  f >c   c |  d    d   c2    |<b-  b-  a  a | g  g f2|>c  c <b- b-| a  a  g2     |>c   c  <b- b- | a  a g2        |f  f> c   c |  d    d   c2    | <b- b-  a  a | g  g f2',
  '@0 T120 S0      L4 O4 a >c  f   f |  f    f   g   f | e   c   f  c |<b- b-a2|>a  a  g  g | f  f  e2     | g   f   d  e  | c  f e8d8c8<b-8|a >c  f   f |  f    f   g   f |  e  c   f  c |<b- b-a2',
  '@1 T120 S0      L8 O3 f>ca4<a>e>c4|<<b->f>d4<<a>f>c4|<<g>eb-4<f>ca4| c4<c4f2| f>ca4<cg>e4|<f>ca4<cg>e<b-| a>f>c4<<g>eb-4|<f>ca4c4 <c4    |f>ca4<a>e>c4|<<b->f>d4<<a>f>c4|<<g>eb-4<f>ca4| c4<c4f2'
]

driver = PSG::Driver.new(:mcp4922, copi: 15, sck: 14, cs: 13, ldac: 12)

# Instead of using a DAC, you can use PWM output
# driver = PSG::Driver.new(:pwm, left: 10, right: 11)

driver.play_mml(tracks)

```

## Playback model

`PSG::Driver#play_mml` keeps the historical synchronous API. Internally it
uses two tasks and a `Task::Queue`:

```text
Sequencer task -> Task::Queue -> Sound task -> PSG ring buffer
```

The sequencer parses MML incrementally and sleeps for each delta in real time
before pushing immediate PSG commands into the queue. The sound task blocks on
`queue.pop`, writes the command to PSG registers as soon as it receives one,
and then immediately waits for the next command.

For asynchronous playback, use `start_mml`:

```ruby
playback = driver.start_mml(tracks)
playback.tempo_scale = 1.25
playback.pause
playback.resume
playback.stop
```

`playback.queue` is public so application tasks can enqueue short live-play or
sound-effect commands. Commands are arrays:

```ruby
playback.queue << [:send_reg, 0, period & 0xff]
playback.queue << [:send_reg, 1, period >> 8]
playback.queue << [:mute, 0, 0]
```

## 🎼 Supported MML Commands

| Command       | Description                                                     |
|---------------|-----------------------------------------------------------------|
| `a`..`g`      | Notes. `+`(`#`) or `-` for sharp and flat. eg: `c+`, `a#`, `d-` |
| `r`           | Rest                                                            |
| `tN`          | Tempo in BPM. eg: `t120`                                        |
| `oN`          | Octave number. eg: `o4` = 4th octave                            |
| `lN`          | Default note length. eg: `l4` = quarter note, `l8` = eighth note|
| `qN`          | Gate time (1–8). `q8` = full length, `q4` = 50%                |
| `<`           | Decrease octave by 1                                            |
| `>`           | Increase octave by 1                                            |
| `pN`          | Pan: `p1` = left, `p8` = center, `p15` = right. `p0` = undefined|
| `vN`          | Volume: `v0`..`v15`.                                            |
| `sN`          | Envelope shape: `s0`..`s15`. Volume will be ignored             |
| `mN`          | Envelope period: `m0`..`m65535`                                 |
| `{ ... }`     | Legato section: `c<{ba#a}` (portamento from "Do" to "La")       |
| `xN`          | Mixer: `x0` = Tone, `x1` = Noise, `x2` = Tone\|Noise            |
| `yN`          | Noise period: `y0`..`y31`                                       |
| `zN`          | Detune: `z0`..`z128`. `z128` lowers one octave                  |
| `@N`          | Timbre: `i0` = Square, `i1` = Triangle, `i2` = Sawtooth, `i3` = Inverse sawtooth |
| `k+N` / `k-N` | Transpose up or down `N` semitones                              |
| `jD,R`        | Vibrato (LFO): depth `D` (halftones), rate `R` (in 0.1Hz units) |
| `.`           | Dotted note. Adds 1/2 length (`l4.` = x1.5, `l4..` = x1.75)     |
| `[ ... ]N`    | Repeat enclosed block `N` times (supports nesting)              |
| `$`           | Segno. Loop start position                                      |
| `\|`          | Barline (purely visual; ignored by parser)                      |


### 🔁 Loop Examples

```text
[cdef]2    => "cdefcdef"
[a[bc]2]3  => "abcbcabcbcabcbc"
[g4.|f4.]2 => "g4.f4.g4.f4."
```

## 📦 Event Format

The `MML#compile_multi` method yields timed events in the following form:

```ruby
yield(delta_ms, track, command, arg0, arg1 = 0)
```

Commands include:

- `:play` : `arg0` is the tone period, `arg1` is sustain duration in ms
- `:mute` : `arg0` is 1 to mute or 0 to unmute
- `:volume` : `arg0` is the channel volume register value
- `:env_period` : `arg0` is the 16-bit envelope period
- `:env_shape` : `arg0` is the envelope shape
- `:legato` : `arg0` is 1 to keep envelope state or 0 to reset on tone update
- `:timbre` : `arg0` is the timbre index
- `:pan` : `arg0` is the pan value
- `:lfo` : `arg0` is depth and `arg1` is rate
- `:mixer` : `arg0` selects tone/noise mode
- `:noise` : `arg0` is the noise period
- `:segno` : loop marker handled by the MML parser

Use `delta_ms` to determine when to trigger each event. `PSG::Playback`
consumes that delta in the sequencer task rather than precomputing all event
times up front.

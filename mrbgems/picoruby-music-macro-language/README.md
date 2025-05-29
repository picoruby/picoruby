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
tracks = {
  0 => "t120 o4 l8 p0 v12 cdefgab>c",
  1 => "t120 o3 l4 p15 v10 g>ceg<c",
  2 => "t120 o2 l2 p8  v8  a2.g2",
}

mml = MML.new
driver = PSG::Driver.new

total_ms = mml.compile_multi(tracks) do |dt, ch, *args|
  case args[0]
  when :lfo
    driver.set_lfo(ch, args[1], args[2])
  when :lfo_off
    driver.set_lfo(ch, 0, 0)
  when :env
    driver.set_envelope(ch, args[1], args[2])
  else
    driver.play_note(ch, *args)
  end
  sleep_ms dt
end

puts "Total duration: #{total_ms} ms"
```

## 🎼 Supported MML Commands

| Command         | Description                                                     |
|-----------------|-----------------------------------------------------------------|
| `a`–`g`         | Notes. Use `+` or `-` for sharp and flat (e.g. `c+`, `d-`)      |
| `r`             | Rest                                                            |
| `tN`            | Tempo in BPM (e.g. `t120`)                                      |
| `oN`            | Octave number (`o4` = 4th octave)                               |
| `lN`            | Default note length (`l4` = quarter note, `l8` = eighth note)   |
| `qN`            | Gate time (1–8). `q8` = full length, `q4` = 50%                 |
| `>`             | Decrease octave by 1                                            |
| `<`             | Increase octave by 1                                            |
| `pN`            | Pan: 0 = left, 8 = center, 15 = right                           |
| `vN`            | Volume: 0–15                                                    |
| `sX,Y`          | Envelope: shape `X` (0–15), period `Y` in milliseconds          |
| `k+N` / `k-N`   | Transpose up or down `N` semitones                              |
| `mD,R`          | Vibrato (LFO): depth `D` (halftones), rate `R` (in 0.1Hz units) |
| `.`             | Dotted note. Adds 1/2 length (`l4.` = 1.5×, `l4..` = 1.75×)     |
| `[ ... ]N`      | Repeat enclosed block `N` times (supports nesting)              |
| `\|`            | Barline (purely visual; ignored by parser)                      |


### 🔁 Loop Examples

```text
[cdef]2    => "cdefcdef"
[a[bc]2]3  => "abcbcabcbcabcbc"
[g4.|f4.]2 => "g4.f4.g4.f4."
```

## 📦 Event Format

The `MML#compile_multi` method yields timed events in the following form:

```ruby
yield(delta_ms, channel, pitch, duration_ms, pan, volume, env_shape, env_period, lfo_depth, lfo_rate)
```
Additionally, control events are emitted when state changes occur:

- `[:lfo, depth, rate]` : Vibrato (LFO) settings updated
- `[:lfo_off]` : LFO disabled
- `[:env, shape, period]` : Envelope settings updated

Use the delta_ms value to determine when to trigger each event.


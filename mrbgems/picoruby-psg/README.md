# picoruby-psg

PSG (Programmable Sound Generator) emulator for PicoRuby.

## Tuning

`PSG.note_to_period(note, round: true)` converts a MIDI note number to a PSG tone period. Notes `0..127` use the current tuning table; notes outside that range are calculated as equal temperament from the current A4 pitch.

```ruby
PSG.note_to_period(60)                 #=> 239
PSG.note_to_period(60, round: false)   #=> 238
```

`PSG.set_tuning(tuning = :equal, pitch: 440)` rebuilds the tuning table and returns the selected tuning symbol. `pitch:` sets the A4 frequency.

```ruby
PSG.set_tuning                         # equal temperament, A4 = 440 Hz
PSG.set_tuning(:equal, pitch: 442)
PSG.set_tuning(:just_f_major, pitch: 440)
```

Pure just intonation tables are available for major and minor keys:

```ruby
:just_c_major       :just_c_minor
:just_c_sharp_major :just_c_sharp_minor
:just_d_flat_major  :just_d_flat_minor
:just_d_major       :just_d_minor
:just_d_sharp_major :just_d_sharp_minor
:just_e_flat_major  :just_e_flat_minor
:just_e_major       :just_e_minor
:just_f_major       :just_f_minor
:just_f_sharp_major :just_f_sharp_minor
:just_g_flat_major  :just_g_flat_minor
:just_g_major       :just_g_minor
:just_g_sharp_major :just_g_sharp_minor
:just_a_flat_major  :just_a_flat_minor
:just_a_major       :just_a_minor
:just_a_sharp_major :just_a_sharp_minor
:just_b_flat_major  :just_b_flat_minor
:just_b_major       :just_b_minor
```

## Timbres and envelopes

Each of the three PSG voices can use one of four waveforms. `PSG::Synth` selects the waveform from the MIDI program number modulo 4; in MML, use `@0` through `@3`:

| MML  | Driver value | Symbol         | Waveform         |
|------|-------------:|----------------|------------------|
| `@0` | 0            | `:square`      | Square           |
| `@1` | 1            | `:triangle`    | Triangle         |
| `@2` | 2            | `:sawtooth`    | Rising sawtooth  |
| `@3` | 3            | `:invsawtooth` | Falling sawtooth |

The same values are available through `PSG::Driver::TIMBRES` when controlling the driver directly:

```ruby
driver.set_timbre(0, PSG::Driver::TIMBRES[:triangle])
driver.set_timbre(1, PSG::Driver::TIMBRES[:sawtooth])
```

The envelope generator uses the AY-compatible period and shape registers. MML provides `m<period>` for the 16-bit envelope period and `s<shape>` for the 4-bit shape. Setting a shape enables envelope volume for that MML channel; a later `v0` through `v15` command returns it to fixed volume.

```ruby
tracks = [
  # Triangle wave with a repeating descending envelope
  "t120 @1 o4 l8 m800 s8 c d e f g a b >c",
  # Rising/falling triangle envelope, continued across notes
  "t120 @2 o3 l4 m500 s14 { c g a f }"
]
```

The equivalent low-level driver setup writes envelope period registers 11 and 12, shape register 13, and sets bit 4 of the voice volume register:

```ruby
voice = 0
period = 800
driver.send_reg(11, period & 0xFF)
driver.send_reg(12, period >> 8)
driver.send_reg(13, 10)
driver.send_reg(8 + voice, 0x10) # use envelope volume
driver.set_legato(voice, 1)      # optional: do not reset on note changes
```

The shape value is the bit field `C A Alt H`:

| Bit | Name      | Effect when set                                     |
|----:|-----------|-----------------------------------------------------|
| 3   | Continue  | Continue after reaching an envelope endpoint        |
| 2   | Attack    | Start at 0 and rise; otherwise start at 15 and fall |
| 1   | Alternate | Reverse direction after each cycle                  |
| 0   | Hold      | Stop at the first endpoint when Continue is set     |

Useful shapes include `s0`/`s4` for one-shot falling/rising envelopes, `s8`/`s12` for repeating sawtooth envelopes, `s10`/`s14` for alternating triangle envelopes, and `s9`/`s13` for falling/rising hold envelopes.

At the 22,050 Hz sample rate, one level step lasts `period / 22_050` seconds, and a complete 16-level sweep is approximately `0.726 * period` milliseconds. A period of zero stops envelope progression. Envelope period and shape are shared PSG registers; changing either affects all voices currently using envelope volume. Writing a shape resets the envelope state, while ordinary note changes reset it unless legato is enabled. MML braces enable legato for the enclosed notes.

Additional PSG-oriented MML controls are:

| MML              | Effect                                    |
|------------------|-------------------------------------------|
| `x0`, `x1`, `x2` | Tone only, noise only, or tone plus noise |
| `y0`..`y31`      | Noise period                              |
| `p0`..`p15`      | Left-to-right pan                         |
| `j1,5`           | Vibrato with 100-cent depth at 0.5 Hz     |

For direct control, `set_lfo(voice, depth, rate)` takes depth in cents (`0..127`) and rate in 0.1 Hz units (`0..255`). The MML `j` depth is expressed as an integer number of semitones, so the current engine range permits `j0` or `j1`.

## MIDI synthesis

`PSG::Synth` consumes events from `MIDIBASE::Router` and maps them to the three PSG voices. It supports note velocity, pitch bend, volume, expression, pan, sustain, programs, and PSG-specific extension events.

`start` creates a PicoRuby task that consumes events from a queue. FemtoRuby/mruby-c is not supported.

```ruby
synth = PSG::Synth.new(driver).start
router = MIDIBASE::Router.new
router.connect(:uart, synth, priority: 100, only: MIDIBASE::CHANNEL_EVENTS)

while true
  router.emit(:uart, midi.getevent)
end
```

Voice ownership includes the Router source. A high-priority live UART source can steal an MML voice, while a lower-priority MML source cannot steal a live voice.

MIDI channel 10 is represented as `PSG::DRUM_CHANNEL` (`9`, because channels are zero-based internally). `PSG::Synth` treats `note_on` on that channel as a drum pad trigger, reserves a physical PSG voice for the sound duration, and asks the C driver to expand one event into PSG sound steps.
If all voices are active, the oldest voice is stolen.
The default note mapping accepts common General MIDI drum notes listed below and other notes are ignored:
- `:kick => 35`/`36`
- `:snare => 38`/`40`
- `:closed_hihat => 42`/`44`
- `:open_hihat => 46`
- `:low_tom => 41`/`43`
- `:mid_tom => 45`/`47`
- `:high_tom => 48`/`50`

Named sounds are global to the PSG module and can be added or replaced at runtime:

```ruby
sound = PSG.set_sound_data(:snare, [
  [220, 3, 15, 35],
  [260, 3, 13, 45],
  [0, 4, 9, 60]
])

driver.sound(:snare)
driver.sound(sound)
```

Each sound step is `[tone_period, noise_period, volume, duration_ms]`. Tone output is enabled when `tone_period` is greater than zero, and noise output is enabled when `noise_period` is greater than zero. `duration_ms` specifies how long the step plays. A final mute step is added automatically. `PSG.sound_data(:snare)` returns the same format, without the internal mute step.

Custom effects use the same API:

```ruby
PSG.set_sound_data(:get_coin, [[180, 0, 15, 40]])
driver.sound(:get_coin)
```

MIDI drum notes can be assigned to any registered sound. Passing `nil` removes an assignment:

```ruby
PSG.assign_drum_sound(60, :get_coin)
PSG.assign_drum_sound(60, nil)
```

MML parsing and sequencing live in the separate `picoruby-midibase-mml` gem. A single Router can combine autonomous or MIDI-clocked MML with live input:

```ruby
clock = MIDIBASE::MML::MIDIClock.new
sequence = MIDIBASE::MML::Sequence.new(tracks, loop: true)
player = MIDIBASE::MML::Player.new(
  sequence,
  clock: clock,
  output: router.output(:mml)
)

router.connect(:mml, synth, priority: 0)
router.connect(:uart, clock, only: MIDIBASE::TRANSPORT_EVENTS)
player.start
```

`picoruby-midibase-mml` can trigger the registered drum sounds by name. These tokens emit MIDI channel 10 events and therefore use the current `assign_drum_sound` mappings:

```ruby
tracks = ["t120 l8 !kick !snare !ch !snare"]
```

## Build note for Raspberry Pi Pico

Use the **prod** build when running PSG on R2P2 for Raspberry Pi Pico.
The debug build is too slow for real-time audio playback and can cause severe audio dropouts.

## MCP4922 DMA output

For MCP4922 output on R2P2, the DMA path is enabled by default. It lets core1 render audio in blocks and feed the MCP4922 PIO TX FIFO by DMA instead of writing one sample at a time from the audio timer callback.

Build normally to use the DMA path:

```sh
rake r2p2:picoruby:pico2:prod
```

To use the legacy blocking PIO write path, disable DMA explicitly:

```sh
PICORUBY_PSG_MCP4922_DMA=off rake r2p2:picoruby:pico2:prod
```

Accepted false values are `0`, `off`, `false`, `no`, and `n`. If the environment variable is not set, R2P2 passes
`PICORUBY_PSG_MCP4922_DMA=ON` to CMake.

When changing this option, check normal playback, stop/restart playback, a longer song, and any tempo-changing or live-control use cases.

## PWM DMA output

For PWM output on R2P2, the DMA path is also enabled by default. It lets core1 render audio in blocks and feed the PWM compare register by DMA instead of updating PWM levels from the audio timer callback.

Build normally to use the DMA path:

```sh
rake r2p2:picoruby:pico2:prod
```

To use the legacy timer callback path, disable DMA explicitly:

```sh
PICORUBY_PSG_PWM_DMA=off rake r2p2:picoruby:pico2:prod
```

Accepted false values are `0`, `off`, `false`, `no`, and `n`. If the environment variable is not set, R2P2 passes `PICORUBY_PSG_PWM_DMA=ON` to CMake.

PWM DMA is paced by the PWM wrap DREQ, so the PWM carrier follows the PSG sample rate. When changing this option, check normal playback, stop/restart playback, and high-frequency noise character on the target speaker or filter circuit.

## Wiring (RP2040 and RP2350)

### MCP492x --- 12bit DAC (SPI)

#### Raspberry Pi Pico (2) (W)
```
         ┣
  GPIO19 ┣───COPI>
  GPIO18 ┣───SCK>
     GND ┣───GND
  GPIO17 ┣───CS>
  GPIO16 ┣───LDAC>
 ━━━━━━━━┛
```

The above is an example.
You can choose a GPIO combination as follows:

|COPI  |SCK   |CS |LDAC|
|------|------|---|---|
|GPIO3 |GPIO2 |any|any|
|GPIO7 |GPIO6 |any|any|
|GPIO11|GPIO10|any|any|
|GPIO15|GPIO14|any|any|
|GPIO19|GPIO18|any|any|
|GPIO23|GPIO22|any|any|
|GPIO27|GPIO26|any|any|

#### MCP4921 (monaural)
```
    3.3V                       3.3V
     │    ┏━━━━━━━┓             │
     └────┫1     8┣───OUTmono>  │
    >CS───┫2     7┣───GND       │
   >SCK───┫3     6┣─────────────┘
  >COPI───┫4     5┣───LDAC<
          ┗━━━━━━━┛
```

#### MCP4922 (stereo)
```
    3.3V                       3.3V
     │    ┏━━━━━━━┓             │
     └────┫1    14┣───OUTleft>  │
         x┫2    13┣─────────────┤
    >CS───┫3    12┣───GND       │
   >SCK───┫4    11┣─────────────┤
  >COPI───┫5    10┣───OUTright> │
         x┫6     9┣─────────────┘
         x┫7     8┣───LDAC<
          ┗━━━━━━━┛
```

### Buzzer (PWM)
```
          ┏━━━━━━━┓
          ┃       ┃   ┏━━━━━━━┓
 >PWM A───┫ Left  ┣───┫330 Ohm┣───GND
          ┃       ┃   ┗━━━━━━━┛
          ┗━━━━━━━┛
          ┏━━━━━━━┓
          ┃       ┃   ┏━━━━━━━┓
 >PWM B───┫ Right ┣───┫330 Ohm┣───GND
          ┃       ┃   ┗━━━━━━━┛
          ┗━━━━━━━┛
```

For stereo output, you must choose a PWM pair that shares the same slice.
This ensures that both left and right channels are updated simultaneously.

|Slice|PWM A |PWM B |
|-----|------|------|
|PWM0 |GPIO0 |GPIO1 |
|PWM1 |GPIO2 |GPIO3 |
|PWM2 |GPIO4 |GPIO5 |
|PWM3 |GPIO6 |GPIO7 |
|PWM4 |GPIO8 |GPIO9 |
|PWM5 |GPIO10|GPIO11|
|PWM6 |GPIO12|GPIO13|
|PWM7 |GPIO14|GPIO15|
|PWM0 |GPIO16|GPIO17|
|PWM1 |GPIO18|GPIO19|
|PWM2 |GPIO20|GPIO21|
|PWM3 |GPIO22|GPIO23|
|PWM4 |GPIO24|GPIO25|
|PWM5 |GPIO26|GPIO27|
|PWM6 |GPIO28|GPIO29|

## Usage

See the examples in `example/` and the `picoruby-midibase-mml` README.

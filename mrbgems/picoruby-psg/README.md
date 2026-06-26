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

MIDI channel 10 is represented as `PSG::DRUM_CHANNEL` (`9`, because channels are zero-based internally). `PSG::Synth` treats `note_on` on that channel as a drum pad trigger, reserves a physical PSG voice for the drum duration, and asks the C driver to expand one event into PSG drum steps.
If all voices are active, the oldest voice is stolen.
The default note mapping accepts common General MIDI drum notes listed below and other notes are ignored:
- `:kick => 35`/`36`
- `:snare => 38`/`40`
- `:closed_hihat => 42`/`44`
- `:open_hihat => 46`
- `:low_tom => 41`/`43`
- `:mid_tom => 45`/`47`
- `:high_tom => 48`/`50`

Drum sound data is global to the PSG module and can be replaced at runtime:

```ruby
PSG.set_drum_data(:snare, [
  [0, 15, 220, 3],
  [35, 13, 260, 3],
  [80, 9, 0, 4],
  [140, 0, 0, 0]
])
```

Each drum step is `[time_ms, volume, tone_period, noise_period]`. `time_ms` is elapsed time from the trigger. Tone output is enabled when `tone_period` is greater than zero, and noise output is enabled when `noise_period` is greater than zero. The older `[time_ms, volume, noise_period]` noise-only form is still accepted.

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

# picoruby-midibase-looper

Tick-based MIDI looper for PicoRuby. It records Note On/Off events at 480 PPQN and emits ordinary MIDIBASE event arrays through a `MIDIBASE::Router`.

The looper does not depend on a particular synthesizer. R2P2's `/bin/looper` routes its sources to `PSG::Synth`, UART MIDI, or USB CDC2 raw MIDI; PSG can be combined with either MIDI output.

```ruby
router = MIDIBASE::Router.new
looper = MIDIBASE::Looper.new(output: router)

router.connect(:midi_in, looper, only: MIDIBASE::CHANNEL_EVENTS)
router.connect(looper.live_source, synth, priority: 0)

i = 0
sources = looper.track_sources
while i < sources.size
  router.connect(sources[i], synth, priority: 100)
  i += 1
end

looper.start
looper.record(voices: 1)
```

## R2P2 command

The default wiring is UART1 MIDI input on RX GPIO5 and MCP4922 audio output:

```text
MIDI device TX -> Pico GPIO5 (UART1 RX)
MCP4922 LDAC/CS/SCK/COPI -> GPIO12/13/14/15
```

Run `looper --help` for all wiring options. Common configurations are:

```sh
# Default MCP4922 output
looper

# PWM audio on GPIO10/11
looper --audio pwm

# MCP4922 plus MIDI output on UART1 TX GPIO4
looper --midi-out uart

# External MIDI sound module only
looper --audio off --midi-out uart

# Raw MIDI bytes over USB CDC2 for WebSerial
looper --audio off --midi-out cdc

# Limit an external MIDI session to eight simultaneous voices/tracks per part
looper --audio off --midi-out uart --polyphony 8
```

MIDI Thru is enabled by default when UART or CDC output is selected.
Use `--no-midi-thru` to send recorded tracks only. Metronome routing is selected with `--click-out audio|midi|both`; audio is the default, with automatic MIDI fallback when local audio is disabled.

The console prints the active MIDI, audio, and metronome routing at startup. A short first recording can be made with:

```text
looper> bars 1
Loop length: 1 bar(s)
looper> countin 1
Count-in: 1 bar(s)
looper> record
Count-in: 1 bar(s) via audio.
Start playing after the count-in.
looper>
Recording 1 bar(s) at 120 BPM...
looper>
Track 1 recorded: voices=1, events=8.
Playback continues automatically.
```

`record` remains asynchronous, so `stop` can cancel an armed or active recording. State changes and the resulting track are reported automatically. In the default `count-in` metronome mode, the click continues through the entire first-track recording and stops when loop playback begins.

## Parts and song arrangement

Each recording creates a new track in the selected part. MIDI channels are
preserved, so changing the keyboard's channel before another `record` layers
that channel over the same part. Track numbers are local to a part.

```text
looper> parts
A*: bars=4, tracks=1
looper> part new B 8
Part B created and selected.
looper> record
Part B armed. Recording starts at the next part boundary.
```

Selecting a different part does not interrupt playback. `play` queues the
selected part, and `record` queues it and starts recording when the currently
playing part ends. Part changes therefore remain on musical boundaries.

Use `part copy A B` to create a variation without copying the immutable MIDI
event storage. Mute and delete state remain independent in the copy. `part
clear B` removes B's tracks but keeps B as an empty part; `part delete B`
removes the part and its arrangement entries while transport is stopped.

An arrangement references parts without duplicating their recordings:

```text
looper> arrange A:2 B:4 A:2 C:1
Arrangement: A:2 B:4 A:2 C:1
looper> song play
Song playback started.
```

Song playback stops after the last repetition. `undo` and `redo` provide one
level of recovery for recordings, track deletion, part edits, arrangement
replacement, and whole-song clearing.

Recording uses sixteenth-note quantization by default. Use `quantize off` before
recording to preserve captured Note On timing, or select `quantize 1/4`,
`quantize 1/8`, `quantize 1/16`, or `quantize 1/8t` for a different grid.
Quantization moves Note On to the nearest grid and moves the paired Note Off by
the same amount.

For local PSG output, `/bin/looper` fixes the click to physical PSG voice 2 and protects it with a priority above channel-10 drums. Music may use all three voices while no click sounds. If the click reclaims voice 2 from a drum, the queued drum steps are cancelled before the click registers are written.

MIDI channel 10 events are recorded unchanged. PSG drum sounds retain voices for their sound-specific duration, so their physical PSG voice stealing is not covered by the looper's logical track voice limit.

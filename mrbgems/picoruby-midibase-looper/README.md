# picoruby-midibase-looper

Tick-based MIDI looper for PicoRuby. It records Note On/Off events at 480 PPQN and emits ordinary MIDIBASE event arrays through a `MIDIBASE::Router`.

The looper does not depend on a particular synthesizer. R2P2's `/bin/looper` routes its sources to `PSG::Synth`, UART MIDI output, or both.

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
```

UART MIDI Thru is enabled by default when MIDI output is selected.
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

`record` remains asynchronous, so `stop` can cancel an armed or active recording. State changes and the resulting track are reported automatically.

MIDI channel 10 events are recorded unchanged. PSG drum sounds retain voices for their sound-specific duration, so their physical PSG voice stealing is not covered by the looper's logical track voice limit.

# JS::WebAudio

A polyphonic WebAudio synthesizer and WebSerial MIDI input for PicoRuby.wasm.
It consumes the event arrays produced by `picoruby-midibase` and works as a
`MIDIBASE::Router` sink.

```ruby
router = MIDIBASE::Router.new
synth = WebAudio::Synth.new
router.connect(:webserial, synth)

input = WebAudio::WebSerialMIDIInput.new(router: router)
synth.resume       # call from a user gesture
input.connect      # opens the browser's WebSerial picker
```

The initial implementation supports Note On/Off, velocity, channel volume,
expression, pan, pitch bend, four oscillator waveforms, Program Change,
All Sound Off, Reset Controllers, and All Notes Off. Sustain pedal is monitored
but intentionally deferred.

## Standalone demo

Build the dedicated runtime and start the local server:

```sh
rake wasm:webaudio:build_prod
rake wasm:webaudio:server
```

Then open `http://localhost:8080`. Enable audio before playing notes, and use
the CDC2 connect button to select PicoRuby's `PicoRuby CDC MIDI` serial port.
WebSerial requires a secure context; `localhost` is accepted by supported
browsers.

For an uncompressed debug runtime, use `rake wasm:webaudio:build_debug`.

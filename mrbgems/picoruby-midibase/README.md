# picoruby-midibase

Shared MIDI 1.0 support for PicoRuby transports and sound generators.

`MIDIBASE::Parser` converts a MIDI byte stream into event arrays,
`MIDIBASE::Clock` tracks BPM and musical position from MIDI Clock, and
`MIDIBASE::VoiceAllocator` assigns polyphonic notes to a fixed number of voices.

Transport classes include `MIDIBASE`, initialize it with
`initialize_midibase`, and implement the private `midi_read_byte` and
`midi_write_byte` methods. This gives every transport the same blocking
`getevent` and symmetric `putevent` API.

Channels are zero-based. Note On with velocity zero is returned as Note Off.
Pitch bend values use the unsigned MIDI range `0..16383` with `8192` as center.

`MIDIBASE::Clock` measures tempo every 24 clock intervals. Its `position`
returns `[bar, beat, tick]`; bars and beats are one-based and ticks are
zero-based. MIDI Clock does not carry a time signature, so configure one when
the transport is created. The default is `[4, 4]`.

System Exclusive messages are buffered without their `F0`/`F7` delimiters.
The default input limit is 1024 bytes. An oversized message produces
`[:system_exclusive_error, :too_large]` after its terminating `F7`.

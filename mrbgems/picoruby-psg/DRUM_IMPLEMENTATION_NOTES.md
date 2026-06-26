# PSG Drum Implementation Notes

## Goal

MIDI channel 10 should be treated as drum pad input in `PSG::Synth`. Instead of calculating drum envelopes in Ruby, the PSG C driver should hold drum sound data and push time-varying noise steps into the PSG processing path. The first target sounds are kick, snare, closed hi-hat, and open hi-hat. Drum data should be globally replaceable through PSG module singleton methods.

## What Was Added

`PSG::DRUM_CHANNEL` was added as internal channel `9`, corresponding to MIDI channel 10.

`PSG.drum_data(kind)` and `PSG.set_drum_data(kind, data)` were added. `kind` accepts symbols such as `:kick`, `:snare`, `:closed_hihat`, `:open_hihat`, and also MIDI drum note numbers. The data format is an array of `[time_ms, volume, noise_period]`, where `time_ms` is elapsed time from the trigger, not a delta.

`PSG::Driver#drum(note_or_kind, velocity = 127)` was added. It maps MIDI note numbers to one of the drum kinds and schedules C-side drum steps.

`PSG::Synth` was changed so `note_on` on `PSG::DRUM_CHANNEL` calls `driver.drum` and does not allocate a normal melodic voice for that note. `note_off` on the drum channel is ignored.

The current implementation also tries to free the last physical PSG voice before a drum hit, so when all three melodic voices are active, voice 2 should be muted and released for drum headroom.

`PSG_PKT_DRUM_STEP` was added as a ring-buffer packet opcode. It currently uses `pkt->reg` as a generation id so stale queued drum steps from an older hit can be ignored.

## What Worked

MIDI channel 10 recognition worked. Debug prints confirmed that the drum channel and note numbers were being received correctly by `PSG::Synth`.

Normal melodic channels continued to play during the investigation, so the general MIDI input path, router path, and PSG melodic voice path were not the primary failure.

A direct register test using `driver.noise_direct(3, 15)` produced audible noise for one second on the target. This showed that PSG noise generation can be heard on the hardware when channel C noise is opened directly.

A temporary full-Ruby script that received MIDI channel 10 and synchronously controlled noise with `noise_direct` also became audible after using short, audible `noise_period` values around `1..7`. That script did not use `PSG::Synth`, `Driver#drum`, or the PSG ring buffer.

The full-Ruby experiment indicated that the MIDI event stream was usable and that audible drum-like noise could be produced if Ruby directly drove the PSG noise registers.

## What Did Not Work

The C-side `Driver#drum` path used by `example/uart_midi_mcp4922.rb` still does not reliably produce audible drums. The user reported either silence or, at most, a very short click-like sound.

Several approaches were tried and were not successful:

- A custom `PSG_PKT_DRUM_STEP` path that stored drum volume and noise period in separate PSG drum fields and mixed an overlay directly in `PSG_calc_sample`.
- Longer default drum envelopes, up to hundreds of milliseconds, to rule out envelopes being too short.
- Broader MIDI note mapping, including fallback to snare for unknown drum note numbers.
- A direct first-step write in `Driver#drum` before queueing the remaining steps.
- Switching back from overlay mixing to channel C noise register control, closer to the known-working `noise_direct` behavior.

The overlay approach was probably a bad match for the known-working behavior. The known-working path used actual PSG channel C noise, mixer, volume, pan, and mute state. The overlay path mixed additional noise outside the normal channel loop and may have had different amplitude or timing behavior.

The ring-buffer path is still suspect. A symptom of “only a very short sound” is consistent with a queued mute or queued volume-zero step arriving immediately after the direct start, or with stale steps from a previous hit interfering with the next hit.

One concrete bug was found: `prepare_drum_voice` originally queued `mute` for voice 2, then `Driver#drum` directly unmuted channel C. The queued mute could run afterward and shut channel C off almost immediately. This was changed to use `mute_direct`, but the overall C path still did not become reliable.

## Current State To Preserve For Handoff

The temporary full-Ruby debug script and public `Driver#noise_direct` API were removed. They were useful for diagnosis but should not be part of the final API.

The non-working C implementation was intentionally left in place for handoff, including:

- `PSG_PKT_DRUM_STEP`
- `PSG::DRUM_CHANNEL`
- `PSG.drum_data`
- `PSG.set_drum_data`
- `PSG::Driver#drum`
- `PSG::Synth` drum-channel handling
- tests that assert MIDI channel 10 goes to `driver.drum` rather than normal voice allocation

The README contains the intended API shape, but the implementation should still be treated as incomplete.

## Likely Next Debug Steps

Start from the known-working behavior, not from the current C drum path. Reintroduce a private direct-noise helper only for debugging if needed, but keep it out of the public API.

First verify `driver.drum(:snare, 127)` without MIDI and without `PSG::Synth`. If that does not produce the same audible result as the old Ruby `noise_direct(3, 15)` test, the problem is inside `Driver#drum` or `PSG_PKT_DRUM_STEP`, not MIDI or voice allocation.

Then verify whether queued drum steps are processed at the expected millisecond intervals. The current RP2040 packet scheduler treats packet `tick` as a relative wait and subtracts it from `g_tick_ms`. If multiple queued drum packets with `tick == 0` or stale generation behavior are wrong, the envelope can collapse to a click.

Check channel C state immediately after a drum hit: mixer bit 2 should disable tone for C, mixer bit 5 should enable noise for C, volume register 10 should be nonzero, pan for C should be centered, and mute bit 2 should be clear.

Decide explicitly whether drums should use physical channel C or an independent overlay. The hardware-tested successful path used physical channel C. If physical channel C is used, `PSG::Synth` must reserve or steal that voice consistently. If overlay is used, the overlay must be calibrated against audible output and should not depend on channel C mute or mixer state.

Do not tune drum timbres until the simple case is stable. A single snare definition like `[[0, 15, 3], [300, 0, 0]]` should be the first C-side acceptance test.


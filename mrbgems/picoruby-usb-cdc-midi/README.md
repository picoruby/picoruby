# picoruby-usb-cdc-midi

Raw MIDI 1.0 output over R2P2's dedicated USB CDC interface 2. This is a
CDC byte stream for WebSerial, not a USB MIDI device.

```ruby
require "usb/cdc/midi"

output = USB::CDC::MIDIOutput.new
router = MIDIBASE::Router.new
router.connect(:uart, output, only: MIDIBASE::WIRE_EVENTS)
router.emit(:uart, [:note_on, 0, 60, 100]) if output.connected?
```

CDC0 remains the shell and CDC1 remains the debug stream. The MIDI interface
is output-only in the initial implementation.

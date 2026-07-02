require "uart-midi"
require "usb/cdc/midi"

input = UART::MIDI.new(unit: :RP2040_UART1, txd_pin: 4, rxd_pin: 5)
output = USB::CDC::MIDIOutput.new
router = MIDIBASE::Router.new
router.connect(:uart, output, only: MIDIBASE::WIRE_EVENTS)

while true
  router.emit(:uart, input.getevent, timestamp_us: input.last_event_timestamp_us)
end

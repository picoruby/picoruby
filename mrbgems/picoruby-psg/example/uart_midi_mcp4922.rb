require "psg"
require "uart-midi"

driver = PSG::Driver.new(:mcp4922, copi: 15, sck: 14, cs: 13, ldac: 12)
synth = PSG::Synth.new(driver).start
controller = PSG::MIDIController.new(synth, logger: STDOUT)
router = MIDIBASE::Router.new
router.connect(:uart, controller, priority: 100, only: MIDIBASE::CHANNEL_EVENTS)
midi = UART::MIDI.new(unit: :RP2040_UART1, txd_pin: 4, rxd_pin: 5)

while true
  router.emit(:uart, midi.getevent)
end

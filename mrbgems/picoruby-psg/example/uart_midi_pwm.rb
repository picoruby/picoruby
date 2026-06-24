require "psg"
require "uart-midi"

driver = PSG::Driver.new(:pwm, left: 10, right: 11)
synth = PSG::Synth.new(driver).start
router = MIDIBASE::Router.new
router.connect(:uart, synth, priority: 100, only: MIDIBASE::CHANNEL_EVENTS)
midi = UART::MIDI.new(
  unit: :PICORB_UART_RP2040_UART0,
  txd_pin: 0,
  rxd_pin: 1
)

while true
  router.emit(:uart, midi.getevent)
end

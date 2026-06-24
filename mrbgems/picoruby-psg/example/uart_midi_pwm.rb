require "psg"
require "uart-midi"

driver = PSG::Driver.new(:pwm, left: 10, right: 11)
player = PSG::MIDIPlayer.new(driver)
midi = UART::MIDI.new(
  unit: :PICORB_UART_RP2040_UART0,
  txd_pin: 0,
  rxd_pin: 1
)

loop do
  player.handle(midi.getevent)
end

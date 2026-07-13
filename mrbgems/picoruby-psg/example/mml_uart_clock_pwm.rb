require "psg"
require "midibase-mml"
require "uart-midi"

tracks = [
  "t120 o4 l8 $ cdef | gab>c |",
  "t120 o3 l4 $ c g | a g |",
]

driver = PSG::Driver.new(:pwm, left: 10, right: 11)
synth = PSG::Synth.new(driver, voice_pools: {mml: 2, uart: 1}).start
controller = PSG::MIDIController.new(synth, logger: STDOUT)
midi = UART::MIDI.new(unit: :RP2040_UART1, txd_pin: 4, rxd_pin: 5)

router = MIDIBASE::Router.new
clock = MIDIBASE::MML::MIDIClock.new
sequence = MIDIBASE::MML::Sequence.new(tracks, loop: true)
player = MIDIBASE::MML::Player.new(
  sequence,
  clock: clock,
  output: router.output(:mml)
)

# Two voices are reserved for MML and one for live UART input.
router.connect(:mml, synth, priority: 0)
router.connect(:uart, controller, priority: 100, only: MIDIBASE::CHANNEL_EVENTS)
router.connect(:uart, clock, only: MIDIBASE::TRANSPORT_EVENTS)

player.start

MIDIBASE::Session.new(player, synth, driver).run do
  router.emit(:uart, midi.getevent)
end

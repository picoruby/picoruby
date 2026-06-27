require "psg"
require "midibase-mml"
require "uart-midi"

tracks = [
  # Bass
  "t120 @0 s0 m1200 o4 l8 $ [ccegbgec|<aa>cegec<a|>ddfacafd|<ggb>dfd<bg>]4",
  # Drums (General MIDI channel 10)
  "t120 l8 $ [!bd !ch !sd !ch !bd !oh !sd !bd]16"
]

driver = PSG::Driver.new(:mcp4922, copi: 15, sck: 14, cs: 13, ldac: 12)
synth = PSG::Synth.new(driver, voice_pools: {mml: 2, uart: 1}).start
controller = PSG::MIDIController.new(synth, logger: STDOUT)
midi = UART::MIDI.new(unit: :RP2040_UART1, txd_pin: 4, rxd_pin: 5)

router = MIDIBASE::Router.new
router.connect(:mml, synth, priority: 0)
router.connect(:uart, controller, priority: 100, only: MIDIBASE::CHANNEL_EVENTS)

sequence = MIDIBASE::MML::Sequence.new(tracks, loop: true)
player = MIDIBASE::MML::Player.new(
  sequence,
  output: router.output(:mml)
).start

while true
  router.emit(:uart, midi.getevent)
end

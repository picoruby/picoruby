require "psg"
require "midibase-mml"

tracks = [
  "t130 @0 s0 m1200 o5 l1 [e- | c | e | b-]4",
  "t130 @0 s0 m1200 o3 l1 [f  | d | f | c ]4",
  # Drums (General MIDI channel 10)
  "t130 l8 [!bd !ch !sd !ch !bd !oh !sd !bd]16"
]

driver = PSG::Driver.new(:mcp4922, copi: 15, sck: 14, cs: 13, ldac: 12)
synth = PSG::Synth.new(driver).start
router = MIDIBASE::Router.new
router.connect(:mml, synth, priority: 0)

sequence = MIDIBASE::MML::Sequence.new(tracks)
player = MIDIBASE::MML::Player.new(
  sequence,
  output: router.output(:mml)
).start

player.join
synth.stop.join
driver.join

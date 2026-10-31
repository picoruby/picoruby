MRuby::Gem::Specification.new('picoruby-midibase-mml') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'MIDI-oriented Music Macro Language sequencer for PicoRuby'

  spec.add_conflict 'picoruby-mrubyc'
  spec.add_dependency 'picoruby-midibase'

  # The MML parser is a character loop in Ruby: compiled with picoruby-aot
  # a Sequence runs about 1.7x faster on a pico2.
  spec.aot = true
  spec.aot_include_widened = true
end

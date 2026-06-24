MRuby::Gem::Specification.new('picoruby-midibase-mml') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'MIDI-oriented Music Macro Language sequencer for PicoRuby'

  spec.add_conflict 'picoruby-mrubyc'
  spec.add_dependency 'picoruby-midibase'
end

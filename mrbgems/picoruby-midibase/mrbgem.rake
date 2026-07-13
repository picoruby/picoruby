MRuby::Gem::Specification.new('picoruby-midibase') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Common MIDI 1.0 parser, encoder, clock, and voice allocator'

  spec.add_conflict 'picoruby-mrubyc'
  spec.add_dependency 'picoruby-machine'
end

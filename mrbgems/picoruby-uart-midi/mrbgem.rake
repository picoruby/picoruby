MRuby::Gem::Specification.new('picoruby-uart-midi') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'MIDI 1.0 transport over UART'

  spec.add_dependency 'picoruby-uart'
  spec.add_dependency 'picoruby-midibase'
end

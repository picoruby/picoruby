MRuby::Gem::Specification.new('picoruby-midibase-looper') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Tick-based MIDI looper for PicoRuby'

  spec.add_conflict 'picoruby-mrubyc'
  spec.add_dependency 'picoruby-midibase'
  spec.add_dependency 'mruby-task'
end

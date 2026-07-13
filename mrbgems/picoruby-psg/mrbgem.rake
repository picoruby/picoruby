MRuby::Gem::Specification.new('picoruby-psg') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Emulator of Programmable Sound Generator (PSG)'

  spec.add_conflict 'picoruby-mrubyc'
  spec.add_conflict 'picoruby-wasm'
  spec.add_dependency 'picoruby-midibase'
end

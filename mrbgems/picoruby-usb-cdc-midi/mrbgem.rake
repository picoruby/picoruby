MRuby::Gem::Specification.new('picoruby-usb-cdc-midi') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Raw MIDI 1.0 output over R2P2 USB CDC interface 2'

  spec.add_conflict 'picoruby-mrubyc'
  spec.add_dependency 'picoruby-machine'
  spec.add_dependency 'picoruby-midibase'
  spec.require_name = 'usb/cdc/midi'
end

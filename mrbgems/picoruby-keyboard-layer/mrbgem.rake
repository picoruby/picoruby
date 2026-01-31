MRuby::Gem::Specification.new('picoruby-keyboard-layer') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Layer switching functionality for keyboard matrix'

  spec.add_dependency 'picoruby-keyboard-matrix'
  spec.add_dependency 'picoruby-usb-hid'
end

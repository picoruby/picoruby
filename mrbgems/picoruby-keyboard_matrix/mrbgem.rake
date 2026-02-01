MRuby::Gem::Specification.new('picoruby-keyboard_matrix') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'GPIO keyboard matrix scanner for R2P2'

  spec.add_dependency 'picoruby-gpio'
  spec.add_dependency 'picoruby-machine'
  spec.add_dependency 'picoruby-usb-hid'
end

MRuby::Gem::Specification.new('picoruby-keyboard_layer') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Layer switching functionality for keyboard matrix'

  if build.vm_mruby?
    spec.add_dependency 'mruby-toplevel-ext', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-toplevel-ext"
  end
  spec.add_dependency 'picoruby-usb-hid'
  spec.add_dependency 'picoruby-keyboard_matrix'
end

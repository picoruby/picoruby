MRuby::Gem::Specification.new('picoruby-keyboard') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Layer switching functionality for keyboard matrix'

  if build.picoruby?
    spec.add_dependency 'mruby-toplevel-ext', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-toplevel-ext"
  elsif build.femtoruby?
    spec.add_dependency 'picoruby-metaprog'
  end
  spec.add_dependency 'picoruby-usb-hid'
  spec.add_dependency 'picoruby-keyboard_matrix'
end

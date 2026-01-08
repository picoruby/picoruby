MRuby::Gem::Specification.new('picoruby-eval') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Kernel#eval implementation for PicoRuby'

  if build.vm_mruby?
    spec.add_dependency 'mruby-binding', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-binding"
  end
end

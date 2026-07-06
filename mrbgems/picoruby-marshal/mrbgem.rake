MRuby::Gem::Specification.new('picoruby-marshal') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Marshal for PicoRuby (for dRuby support)'

  if build.picoruby?
    spec.add_dependency 'mruby-pack', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-pack"
  elsif build.femtoruby?
    spec.add_dependency 'picoruby-pack'
  end
end

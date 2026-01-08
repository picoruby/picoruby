MRuby::Gem::Specification.new('picoruby-mqtt') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'MQTT client for PicoRuby'

  spec.add_dependency 'picoruby-socket'

  if build.vm_mruby?
    spec.add_dependency 'mruby-pack', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-pack"
  elsif build.vm_mrubyc?
    spec.add_dependency 'picoruby-pack'
  end
end

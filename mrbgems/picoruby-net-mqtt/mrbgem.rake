MRuby::Gem::Specification.new('picoruby-net-mqtt') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'MQTT client for PicoRuby'

  spec.add_dependency 'picoruby-socket'
  spec.add_dependency 'picoruby-machine'

  spec.require_name = 'net/mqtt'

  if build.vm_mruby?
    spec.add_dependency 'mruby-pack', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-pack"
  elsif build.vm_mrubyc?
    spec.add_dependency 'picoruby-pack'
  end
end

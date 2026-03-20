MRuby::Gem::Specification.new('picoruby-net-websocket') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'WebSocket client and server for PicoRuby'

  spec.require_name = 'net/websocket'

  if build.vm_mruby?
    spec.add_dependency 'mruby-pack', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/picoruby-pack"
  elsif build.vm_mrubyc?
    spec.add_dependency 'picoruby-pack'
  end
  spec.add_dependency 'picoruby-socket'
  spec.add_dependency 'picoruby-base64'
  spec.add_dependency 'picoruby-rng'
  spec.add_dependency 'picoruby-mbedtls'
end

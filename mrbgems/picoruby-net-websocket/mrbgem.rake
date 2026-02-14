MRuby::Gem::Specification.new('picoruby-net-websocket') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'WebSocket client for PicoRuby'

  spec.require_name = 'net/websocket'

  spec.add_dependency 'picoruby-socket'
  spec.add_dependency 'picoruby-base64'
  spec.add_dependency 'picoruby-rng'
end

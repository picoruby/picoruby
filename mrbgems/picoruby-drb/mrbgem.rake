MRuby::Gem::Specification.new('picoruby-drb') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'dRuby (Distributed Ruby) for PicoRuby (mruby VM only)'

  spec.add_dependency 'picoruby-mruby' # picoruby-drb doesn't support mruby/c
  spec.add_dependency 'picoruby-marshal'

  if build.cc.command =~ /emcc/
    spec.add_dependency 'picoruby-wasm'          # includes JS::WebSocket
  else
    spec.add_dependency 'picoruby-socket'        # for DRb::DRbTCPServer
    spec.add_dependency 'picoruby-net-websocket' # for DRb::WebSocket
  end
end

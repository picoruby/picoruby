MRuby::Gem::Specification.new('picoruby-drb-websocket') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'WebSocket protocol for picoruby-drb'

  spec.add_dependency 'picoruby-drb'

  if build.cc.command =~ /emcc/
    spec.add_dependency 'picoruby-wasm'
  else
    spec.add_dependency 'picoruby-net-websocket'
  end
end

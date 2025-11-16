MRuby::Gem::Specification.new('picoruby-coap') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'CoAP (Constrained Application Protocol) client and server for PicoRuby'

  spec.add_dependency 'picoruby-socket'
end

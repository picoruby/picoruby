MRuby::Gem::Specification.new('picoruby-mdns') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'mDNS (Multicast DNS) client and responder for PicoRuby'

  spec.add_dependency 'picoruby-socket'
end

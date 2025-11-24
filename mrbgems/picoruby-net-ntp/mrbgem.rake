MRuby::Gem::Specification.new('picoruby-net-ntp') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'NTP (Network Time Protocol) client for PicoRuby'
  spec.description = 'Simple NTP client implementation compatible with CRuby'

  spec.require_name = 'net/ntp'

  spec.add_dependency 'picoruby-socket'
  spec.add_dependency 'picoruby-time'
end

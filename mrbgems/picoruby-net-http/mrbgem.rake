MRuby::Gem::Specification.new('picoruby-net-http') do |spec|
  spec.license = 'MIT'
  spec.author  = 'PicoRuby developers'
  spec.summary = 'CRuby-compatible Net::HTTP implementation for PicoRuby'
  spec.description = 'HTTP client library for PicoRuby, providing Net::HTTP interface compatible with CRuby'

  # Dependency on picoruby-socket-class for TCP/SSL socket support
  spec.add_dependency 'picoruby-socket-class'
end

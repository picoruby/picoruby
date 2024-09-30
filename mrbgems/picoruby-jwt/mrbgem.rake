MRuby::Gem::Specification.new('picoruby-jwt') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'JWT (RFC7519) implementation for PicoRuby'

  spec.add_dependency 'picoruby-json'
  spec.add_dependency 'picoruby-mbedtls'
  spec.add_dependency 'picoruby-base64'
end



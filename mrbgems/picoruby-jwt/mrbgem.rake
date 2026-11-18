MRuby::Gem::Specification.new('picoruby-jwt') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'JWT (RFC7519) implementation for PicoRuby'

  spec.add_dependency 'picoruby-json'
  spec.add_dependency 'picoruby-mbedtls'
  spec.add_dependency 'picoruby-base64'
  if build.picoruby?
    # String#force_encoding: a decoded segment is binary, its JSON is UTF-8
    spec.add_dependency 'mruby-encoding', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-encoding"
  end
end



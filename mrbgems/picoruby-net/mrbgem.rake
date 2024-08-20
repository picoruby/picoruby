MRuby::Gem::Specification.new('picoruby-net') do |spec|
  spec.license = 'MIT'
  spec.author  = 'Ryo Kajiwara'
  spec.summary = 'Network functionality for PicoRuby'
  spec.add_dependency 'picoruby-cyw43'
  spec.add_dependency 'picoruby-mbedtls'
end

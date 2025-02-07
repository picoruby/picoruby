MRuby::Gem::Specification.new('picoruby-net') do |spec|
  spec.license = 'MIT'
  spec.authors = ['Ryo Kajiwara', 'HASUMI Hitoshi']
  spec.summary = 'Network functionality for PicoRuby'

  spec.add_dependency 'picoruby-time-class'
  spec.add_dependency 'picoruby-pack'

  spec.posix

  if build.name == 'host'
    spec.mruby.linker.flags_after_libraries << '-lssl'
    spec.mruby.linker.flags_after_libraries << '-lcrypto'
  else
    # TODO refactor
    # cyw43 is only for pico_w but picoruby-net is also for POSIX
    # spec.add_dependency 'picoruby-cyw43'
    spec.add_dependency 'picoruby-mbedtls'
  end

end

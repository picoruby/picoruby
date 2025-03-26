MRuby::Gem::Specification.new('picoruby-mqtt') do |spec|
  spec.license = 'MIT'
  spec.authors = ['Ryosuke Uchida']
  spec.summary = 'MQTT client for PicoRuby'

  spec.add_dependency 'picoruby-time-class'
  spec.add_dependency 'picoruby-pack'

  spec.posix

  if build.posix?
    spec.mruby.linker.flags_after_libraries << '-lssl'
    spec.mruby.linker.flags_after_libraries << '-lcrypto'
  else
    # TODO refactor
    # cyw43 is only for pico_w but picoruby-net is also for POSIX
    # spec.add_dependency 'picoruby-cyw43'
    spec.add_dependency 'picoruby-mbedtls'
  end

end

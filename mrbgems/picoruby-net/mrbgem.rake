MRuby::Gem::Specification.new('picoruby-net') do |spec|
  spec.license = 'MIT'
  spec.authors = ['Ryo Kajiwara', 'HASUMI Hitoshi']
  spec.summary = 'Network functionality for PicoRuby'

  spec.add_dependency 'picoruby-time-class'
  spec.add_dependency 'picoruby-pack'

  build.porting(dir)

  unless cc.defines.include?('PICORUBY_PLATFORM=posix')
    # TODO refactor
    # cyw43 is only for pico_w but picoruby-net is also for PISIX
    # spec.add_dependency 'picoruby-cyw43'
    spec.add_dependency 'picoruby-mbedtls'
  end

end

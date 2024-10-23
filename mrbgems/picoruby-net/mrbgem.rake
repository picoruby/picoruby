MRuby::Gem::Specification.new('picoruby-net') do |spec|
  spec.license = 'MIT'
  spec.authors = ['Ryo Kajiwara', 'HASUMI Hitoshi']
  spec.summary = 'Network functionality for PicoRuby'

  spec.add_dependency 'picoruby-time-class'
  spec.add_dependency 'picoruby-pack'

  if %w(host no-libc-host).include?(cc.build.name)
    %w(tcp udp).each do |proto|
      src = "#{dir}/ports/posix/#{proto}.c"
      obj = objfile(src.pathmap("#{build_dir}/ports/posix/%n"))
      build.libmruby_objs << obj
      file obj => src do |f|
        cc.run f.name, f.prerequisites.first
      end
    end
  else
    # TODO refactor
    # cyw43 is only for pico_w but picoruby-net is also for PISIX
    # spec.add_dependency 'picoruby-cyw43'
    spec.add_dependency 'picoruby-mbedtls'
  end

end

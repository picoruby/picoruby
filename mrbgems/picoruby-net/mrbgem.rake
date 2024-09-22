MRuby::Gem::Specification.new('picoruby-net') do |spec|
  spec.license = 'MIT'
  spec.author  = 'Ryo Kajiwara'
  spec.summary = 'Network functionality for PicoRuby'

  if %w(host no-libc-host).include?(cc.build.name)
    src = "#{dir}/ports/posix/net.c"
    obj = objfile(src.pathmap("#{build_dir}/ports/posix/%n"))
    build.libmruby_objs << obj
    file obj => src do |f|
      cc.run f.name, f.prerequisites.first
    end
  else
    # TODO refactor
    spec.add_dependency 'picoruby-cyw43'
    spec.add_dependency 'picoruby-mbedtls'
  end
end

MRuby::Gem::Specification.new('picoruby-rng') do |spec|
  spec.license = 'MIT'
  spec.author  = 'Ryo Kajiwara'
  spec.summary = 'Random Number Generator for PicoRuby'

  if %w(host no-libc-host).include?(cc.build.name)
    src = "#{dir}/ports/posix/rng.c"
    obj = objfile(src.pathmap("#{build_dir}/ports/posix/%n"))
    build.libmruby_objs << obj
    file obj => src do |f|
      cc.run f.name, f.prerequisites.first
    end
  end
end

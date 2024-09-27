MRuby::Gem::Specification.new('picoruby-io-console') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'IO class'

  spec.require_name = 'io/console'

  if %w(host no-libc-host).include?(cc.build.name)
    src = "#{dir}/ports/posix/io-console.c"
    obj = objfile(src.pathmap("#{build_dir}/ports/posix/%n"))
    build.libmruby_objs << obj
    file obj => src do |f|
      cc.run f.name, f.prerequisites.first
    end
  end
end



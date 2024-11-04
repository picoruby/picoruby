MRuby::Gem::Specification.new('picoruby-io-console') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'IO class'

  spec.add_dependency 'picoruby-env'

  spec.require_name = 'io/console'

  if cc.defines.include?("PICORUBY_POSIX")
    src = "#{dir}/ports/posix/io-console.c"
    obj = objfile(src.pathmap("#{build_dir}/ports/posix/%n"))
    build.libmruby_objs << obj
    file obj => src do |f|
      cc.run f.name, f.prerequisites.first
    end
  end
end



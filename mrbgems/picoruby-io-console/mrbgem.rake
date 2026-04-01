MRuby::Gem::Specification.new('picoruby-io-console') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'IO class'

  spec.add_dependency 'picoruby-env'

  if build.name == "nrf52"
    src = "#{dir}/ports/nrf52/io-console.c"
    obj = src.relative_path_from(dir).pathmap("#{build_dir}/%X.o")
    spec.objs << obj
  end

  spec.require_name = 'io/console'
end


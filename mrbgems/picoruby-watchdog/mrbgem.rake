MRuby::Gem::Specification.new('picoruby-watchdog') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Watchdog class'

  if build.name == "nrf52"
    src = "#{dir}/ports/nrf52/watchdog.c"
    obj = src.relative_path_from(dir).pathmap("#{build_dir}/%X.o")
    spec.objs << obj
  end
end


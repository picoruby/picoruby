MRuby::Gem::Specification.new('picoruby-gpio') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'GPIO class / General peripherals'

  if build.name == "nrf52"
    src = "#{dir}/ports/nrf52/gpio.c"
    obj = src.relative_path_from(dir).pathmap("#{build_dir}/%X.o")
    spec.objs << obj
  end
end

MRuby::Gem::Specification.new('picoruby-env') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'ENV'

  if build.name == "nrf52"
    src = "#{dir}/ports/nrf52/env.c"
    obj = src.relative_path_from(dir).pathmap("#{build_dir}/%X.o")
    spec.objs << obj
  end

  if build.vm_mruby?
    # Workaround:
    #   Locate picoruby-mruby at the (almost) top of gem_init.c
    #   to define Kernel#require earlier than other gems
    spec.add_dependency 'picoruby-mruby'
  end
end

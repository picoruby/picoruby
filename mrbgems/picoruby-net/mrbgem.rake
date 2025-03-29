MRuby::Gem::Specification.new('picoruby-net') do |spec|
  spec.license = 'MIT'
  spec.authors = ['Ryo Kajiwara', 'HASUMI Hitoshi']
  spec.summary = 'Network functionality for PicoRuby'

  spec.add_dependency 'picoruby-time-class'
  spec.add_dependency 'picoruby-pack'
  spec.add_dependency 'picoruby-mbedtls'

  spec.cc.defines << 'PICO_CYW43_ARCH_POLL=1'

  spec.cc.include_paths << "/home/hasumi/pico/pico-sdk/lib/lwip/src/include"
  spec.cc.include_paths << "/home/hasumi/pico/pico-sdk/lib/lwip/contrib/ports/unix/port/include"
  spec.cc.include_paths << "/home/hasumi/work/R2P2/include-net"
  #spec.cc.include_paths << "#{build.gems['picoruby-mbedtls'].dir}/mbedtls/include"
  spec.cc.include_paths << "/home/hasumi/work/picoruby/mrbgems/picoruby-mbedtls/mbedtls/include"

  Dir.glob("/home/hasumi/pico/pico-sdk/lib/lwip/src/**/*.c").each do |src|
    next if src.end_with?('makefsdata.c')
    obj = src.relative_path_from("/home/hasumi/pico/pico-sdk/lib/lwip/src").pathmap("#{build_dir}/lwip/%X.o")
    spec.objs << obj
    task obj => src do |t|
      cc.run t.name, t.prerequisites.first
    end
  end
  if build.posix?
    src = "/home/hasumi/pico/pico-sdk/lib/lwip/contrib/ports/unix/port/sys_arch.c"
    obj = src.relative_path_from("/home/hasumi/pico/pico-sdk/lib/lwip/src").pathmap("#{build_dir}/lwip/%X.o")
    task obj => src do |t|
      cc.run t.name, t.prerequisites.first
    end
    spec.objs << obj
  end

  spec.posix
end

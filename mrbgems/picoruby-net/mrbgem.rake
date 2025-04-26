MRuby::Gem::Specification.new('picoruby-net') do |spec|
  spec.license = 'MIT'
  spec.authors = ['Ryo Kajiwara', 'HASUMI Hitoshi']
  spec.summary = 'Network functionality for PicoRuby'

  if build.vm_mrubyc?
    spec.add_dependency 'picoruby-time-class'
  elsif build.vm_mruby?
    spec.add_dependency 'mruby-time'
  end
  spec.add_dependency 'picoruby-pack'
  spec.add_dependency 'picoruby-mbedtls'

  LWIP_VERSION = "STABLE-2_2_0_RELEASE"
  lwip_dir = "#{dir}/lib/lwip"
  unless File.directory?("#{dir}/lib/lwip")
    sh "git clone -b #{LWIP_VERSION} https://github.com/lwip-tcpip/lwip #{lwip_dir}"
  end

  spec.cc.defines << 'PICO_CYW43_ARCH_POLL=1'

  spec.cc.include_paths << "#{lwip_dir}/src/include"
  spec.cc.include_paths << "#{lwip_dir}/contrib/ports/unix/port/include"
  spec.cc.include_paths << "#{build.gems['picoruby-mbedtls'].dir}/mbedtls/include"

  Dir.glob("#{lwip_dir}/src/**/*.c").each do |src|
    next if src.end_with?('makefsdata.c')
    obj = src.relative_path_from(dir).pathmap("#{build_dir}/%X.o")
    spec.objs << obj
  end

  if build.posix?
    src = "#{lwip_dir}/contrib/ports/unix/port/sys_arch.c"
    obj = src.relative_path_from(dir).pathmap("#{build_dir}/%X.o")
    spec.objs << obj
  end

  spec.posix
end

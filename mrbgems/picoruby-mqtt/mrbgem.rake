MRuby::Gem::Specification.new('picoruby-mqtt') do |spec|
  spec.license = 'MIT'
  spec.authors = ['Ryosuke Uchida']
  spec.summary = 'MQTT client for PicoRuby'

  spec.add_dependency 'picoruby-net'
  # WIP: TLS/SSL support is planned but not yet implemente
  spec.add_dependency 'picoruby-mbedtls'
  spec.add_dependency 'picoruby-cyw43'

  spec.cc.defines << 'PICO_CYW43_ARCH_POLL=1'

  spec.cc.include_paths << "#{build.gems['picoruby-net'].dir}/include"
  spec.cc.include_paths << "#{build.gems['picoruby-mbedtls'].dir}/mbedtls/include"
  
  net_gem = build.gems['picoruby-net']
  lwip_dir = "#{net_gem.dir}/lib/lwip"
  spec.cc.include_paths << "#{lwip_dir}/src/include"
  spec.cc.include_paths << "#{lwip_dir}/contrib/ports/unix/port/include"

  if build.vm_mrubyc?
    mrubyc_dir = "#{build.gems['picoruby-mrubyc'].dir}/lib/mrubyc"
    spec.cc.include_paths << "#{mrubyc_dir}/src"
  end

  spec.objs << "#{dir}/src/mqtt.o"
  file "#{dir}/src/mqtt.o" => "#{dir}/src/mqtt.c" do |t|
    cc.run t.name, t.prerequisites.first
  end

  if build.vm_mruby?
    spec.objs << "#{dir}/src/mruby/mqtt.o"
    file "#{dir}/src/mruby/mqtt.o" => "#{dir}/src/mruby/mqtt.c" do |t|
      cc.run t.name, t.prerequisites.first
    end
  elsif build.vm_mrubyc?
    spec.objs << "#{dir}/src/mrubyc/mqtt.o"
    file "#{dir}/src/mrubyc/mqtt.o" => "#{dir}/src/mrubyc/mqtt.c" do |t|
      cc.run t.name, t.prerequisites.first
    end
  end

  spec.posix
end

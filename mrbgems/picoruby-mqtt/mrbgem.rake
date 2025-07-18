MRuby::Gem::Specification.new('picoruby-mqtt') do |spec|
  spec.license = 'MIT'
  spec.authors = ['Ryosuke Uchida']
  spec.summary = 'MQTT client for PicoRuby'

  spec.add_dependency 'picoruby-net'
  spec.add_dependency 'picoruby-pack'
  if build.vm_mrubyc?
    spec.add_dependency 'picoruby-time-class'
  elsif build.vm_mruby?
    spec.add_dependency 'mruby-time'
  end

  lwip_dir = "#{build.gems['picoruby-net'].dir}/lib/lwip"
  spec.cc.include_paths << "#{lwip_dir}/src/include"
  spec.cc.include_paths << "#{lwip_dir}/contrib/ports/unix/port/include"
  spec.cc.include_paths << "#{lwip_dir}/src/include/lwip/apps"
  spec.cc.include_paths << "#{build.gems['picoruby-net'].dir}/include"
  spec.cc.include_paths << "#{build.gems['picoruby-mbedtls'].dir}/mbedtls/include"

  spec.objs << "#{dir}/src/mqtt.o"
  file "#{dir}/src/mqtt.o" => "#{dir}/src/mqtt.c" do |t|
    cc.run t.name, t.prerequisites.first
  end

  if build.vm_mrubyc?
    spec.objs << "#{dir}/src/mrubyc/mqtt.o"
    file "#{dir}/src/mrubyc/mqtt.o" => "#{dir}/src/mrubyc/mqtt.c" do |t|
      cc.run t.name, t.prerequisites.first
    end
  elsif build.vm_mruby?
    spec.objs << "#{dir}/src/mruby/mqtt.o"
    file "#{dir}/src/mruby/mqtt.o" => "#{dir}/src/mruby/mqtt.c" do |t|
      cc.run t.name, t.prerequisites.first
    end
  end

  spec.posix
end

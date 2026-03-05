MRuby::Gem::Specification.new('picoruby-mqtt-lwip') do |spec|
  spec.license = 'MIT'
  spec.authors = ['Ryosuke Uchida']
  spec.summary = 'lwIP native MQTT client for PicoRuby'
  spec.require_name = 'net/mqtt'

  spec.add_dependency 'picoruby-socket'
  spec.add_dependency 'picoruby-pack'
  if build.vm_mrubyc?
    spec.add_dependency 'picoruby-time'
  elsif build.vm_mruby?
    spec.add_dependency 'mruby-time'
  end

  lwip_dir = "#{MRUBY_ROOT}/mrbgems/picoruby-socket/lib/lwip"
  spec.cc.include_paths << "#{lwip_dir}/src/include"
  spec.cc.include_paths << "#{lwip_dir}/contrib/ports/unix/port/include"
  spec.cc.include_paths << "#{lwip_dir}/src/include/lwip/apps"
  spec.cc.include_paths << "#{MRUBY_ROOT}/mrbgems/picoruby-socket/include"
  spec.cc.include_paths << "#{MRUBY_ROOT}/mrbgems/picoruby-mbedtls/include"

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

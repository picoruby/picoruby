MRuby::Gem::Specification.new('picoruby-net-mqtt-femto') do |spec|
  spec.license = 'MIT'
  spec.authors = ['Ryosuke Uchida']
  spec.summary = 'lwIP native MQTT client for PicoRuby (RP2040 optimized)'
  spec.require_name = 'net/mqtt'

  spec.add_dependency 'picoruby-socket'
  spec.add_dependency 'picoruby-pack'
  spec.add_dependency 'picoruby-time'

  # Note: C implementation is in ports/rp2040/mqtt_lwip.c
  # and will be automatically included by CMake build system
end

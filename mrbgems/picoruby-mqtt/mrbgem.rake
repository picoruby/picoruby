MRuby::Gem::Specification.new('picoruby-mqtt') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'MQTT client for PicoRuby'

  spec.add_dependency 'picoruby-socket'
end

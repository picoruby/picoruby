MRuby::Gem::Specification.new('picoruby-ble') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'BLE class'
  spec.add_dependency 'picoruby-cyw43'
  spec.add_dependency 'picoruby-task-ext'
  spec.add_dependency 'picoruby-mbedtls'
end



MRuby::Gem::Specification.new('picoruby-ble') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'BLE class'
  spec.add_dependency 'picoruby-mbedtls'
  spec.add_dependency 'picoruby-cyw43' unless build.posix? || build.name =~ /esp32|xtensa-esp/
end

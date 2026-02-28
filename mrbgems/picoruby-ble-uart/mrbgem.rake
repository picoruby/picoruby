MRuby::Gem::Specification.new('picoruby-ble-uart') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'BLE UART (Nordic UART Service) wrapper for PicoRuby'

  spec.add_dependency 'picoruby-ble'
end

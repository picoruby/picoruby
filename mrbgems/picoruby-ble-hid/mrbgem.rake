MRuby::Gem::Specification.new('picoruby-ble-hid') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'BLE HID (Human Interface Device) wrapper for PicoRuby'

  spec.add_dependency 'picoruby-ble'
end

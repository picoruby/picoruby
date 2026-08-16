MRuby::Gem::Specification.new('picoruby-ble') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'BLE class'
  # picoruby-cyw43 ships only ports/rp2040, so linking it elsewhere fails.
  spec.add_dependency 'picoruby-cyw43' if %w(pico_w pico2_w).include?(ENV['PICORB_BOARD'])
  spec.add_dependency 'picoruby-mbedtls'
  spec.add_dependency 'mruby-task' if build.picoruby?
end

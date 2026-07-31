MRuby::Gem::Specification.new('picoruby-ble') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'BLE class'
  # picoruby-cyw43 ships only ports/rp2040, while its src/cyw43.c compiles for
  # every target, so depending on it unconditionally leaves CYW43_GPIO_read /
  # CYW43_GPIO_write / CYW43_arch_init_with_country undefined at link time on
  # any board without that port. Guarded the same way picoruby-network guards
  # the identical dependency.
  if %w(pico_w pico2_w).include?(ENV['PICORB_BOARD'])
    spec.add_dependency 'picoruby-cyw43'
  end
  spec.add_dependency 'picoruby-mbedtls'
  spec.add_dependency 'mruby-task' if build.picoruby?
end



MRuby::Gem::Specification.new('picoruby-env') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'ENV'

  # The nRF52 port persists to /etc/env through littlefs's C API. Declared
  # here rather than reached with a relative include, the way picoruby-uart
  # declares picoruby-irq.
  if build.platform?(:nrf52)
    spec.add_dependency 'picoruby-littlefs'
    spec.cc.include_paths << "#{MRUBY_ROOT}/mrbgems/picoruby-littlefs/include"
    spec.cc.include_paths << "#{MRUBY_ROOT}/mrbgems/picoruby-littlefs/lib/littlefs"
  end

  if build.picoruby?
    # Workaround:
    #   Locate picoruby-mruby at the (almost) top of gem_init.c
    #   to define Kernel#require earlier than other gems
    spec.add_dependency 'picoruby-mruby'
  end
end

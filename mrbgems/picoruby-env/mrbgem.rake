MRuby::Gem::Specification.new('picoruby-env') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'ENV'

  # The nRF52 port persists to /etc/env through littlefs's C API.
  #
  # Include paths only -- deliberately NOT add_dependency. littlefs
  # depends on picoruby-time, which depends back on picoruby-env, so
  # declaring the edge closes a cycle and the gem graph fails to sort.
  # The port calls littlefs_ensure_mounted()/littlefs_get_lfs(), which
  # the linker resolves from the same image; it needs the headers, not an
  # ordering constraint.
  if build.platform?(:nrf52)
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

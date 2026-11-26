MRuby::Gem::Specification.new('picoruby-random') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Random: random bytes and numbers from the platform'

  if build.picoruby?
    # The mruby VM takes Random from mruby-random and Random.urandom from
    # mruby-random-urandom. On a board, this gem gives the mrb_hal_urandom()
    # that Random.urandom reads (src/mruby/random.c). A POSIX build takes the
    # posix port of mruby-random-urandom. mruby-random comes first by gemdir:
    # the core: dependency of mruby-random-urandom would be looked up in the
    # mrbgems/ of PicoRuby.
    spec.add_dependency 'mruby-random', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-random"
    spec.add_dependency 'mruby-random-urandom', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-random-urandom"
  end
end

MRuby::Gem::Specification.new('picoruby-vfs') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Virtual-File-System-like wrapper for filesystems'

  spec.add_dependency 'picoruby-env'
  spec.add_dependency 'picoruby-time'

  if ENV['TEST_TASK']
    # The tests mount a RAM littlefs (ports/posix) at "/". A production
    # build picks its own driver, so the dependency is for `rake test:gems:*`
    # only (the same arrangement as picoruby-sqlite3). The suite runs on
    # PicoRuby only: on the FemtoRuby test build littlefs conflicts with
    # picoruby-posix-io, and a dependency on picoruby-mruby keeps the gem
    # out of that build's test set.
    spec.add_dependency 'picoruby-mruby'
    spec.add_dependency 'picoruby-littlefs'
    # The host test binary also carries mruby-dir and mruby-io (the
    # mruby-posix gembox). This gem reopens File and Dir in Ruby; whichever
    # gem initializes last owns the methods both define, so this gem must
    # come after them: mkdir would otherwise run on the host filesystem.
    # That binary only: TEST_TASK is set in every process of a test run,
    # so it cannot tell the gem collection apart; test.rake sets
    # PICORB_TEST_BINARY for the build of the test binary alone.
    if build.picoruby? && ENV['PICORB_TEST_BINARY']
      spec.add_dependency 'mruby-dir', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-dir"
      spec.add_dependency 'mruby-io', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-io"
    end
  end
end

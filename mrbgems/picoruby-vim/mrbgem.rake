MRuby::Gem::Specification.new('picoruby-vim') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'vim-like text editor'
  spec.add_dependency 'picoruby-editor'

  if ENV['TEST_TASK']
    # Vim reads and writes files through File, which picoruby-vfs routes to
    # a mounted volume; the tests mount a RAM littlefs (ports/posix) at "/".
    # For `rake test:gems:*` only; a device build brings its own filesystem.
    # The suite runs on PicoRuby only (littlefs conflicts with
    # picoruby-posix-io on the FemtoRuby test build; instance_variable_get
    # is mruby-metaprog), and a dependency on picoruby-mruby keeps the gem
    # out of that build's test set.
    spec.add_dependency 'picoruby-mruby'
    spec.add_dependency 'picoruby-vfs'
    spec.add_dependency 'picoruby-littlefs'
  end
end

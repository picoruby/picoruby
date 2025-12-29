MRuby::Gem::Specification.new('picoruby-funicular') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Browser application framework with VDOM for PicoRuby.wasm'

  unless ENV['TEST_TASK']
    spec.add_dependency 'picoruby-wasm'
  end
  spec.add_dependency 'picoruby-json'
  spec.add_dependency 'mruby-hash-ext', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-hash-ext"
  spec.add_dependency 'mruby-array-ext', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-array-ext"
  spec.add_dependency 'mruby-string-ext', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-string-ext"
  spec.add_dependency 'mruby-metaprog', gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-metaprog"
end

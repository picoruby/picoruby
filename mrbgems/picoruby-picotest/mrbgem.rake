MRuby::Gem::Specification.new('picoruby-picotest') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Minitest-like testing framework for PicoRuby'

  if build.vm_mrubyc?
    spec.add_dependency 'picoruby-metaprog'
    spec.add_dependency 'picoruby-dir'
    spec.add_dependency 'picoruby-posix-io'
  else
    spec.add_dependency 'mruby-metaprog'
  end
  spec.add_dependency 'picoruby-json'
  spec.add_dependency 'picoruby-env'
end

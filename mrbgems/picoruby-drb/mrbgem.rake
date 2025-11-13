MRuby::Gem::Specification.new('picoruby-drb') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'dRuby (Distributed Ruby) for PicoRuby'

  spec.add_dependency 'picoruby-marshal'
  spec.add_dependency 'picoruby-net'

  # Add test files
  spec.test_rbfiles = Dir.glob("#{dir}/test/*.rb")
end

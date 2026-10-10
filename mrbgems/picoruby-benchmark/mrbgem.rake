MRuby::Gem::Specification.new('picoruby-benchmark') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Benchmark module for PicoRuby, modeled on the CRuby Benchmark library'

  spec.add_dependency 'picoruby-machine'
  spec.add_dependency 'picoruby-picorubyvm'
end

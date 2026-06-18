MRuby::Gem::Specification.new('picoruby-markdown') do |spec|
  spec.license = 'MIT'
  spec.author  = 'hasumikin'

  spec.test_rbfiles = [] unless build.vm_mruby? && !build.wasm?

  spec.add_dependency 'picoruby-yaml'
end

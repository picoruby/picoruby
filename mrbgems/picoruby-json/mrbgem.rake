MRuby::Gem::Specification.new('picoruby-json') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'JSON parser for PicoRuby'

  spec.test_rbfiles = [] unless build.vm_mruby? && !build.wasm?
end

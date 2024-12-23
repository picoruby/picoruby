MRuby::Gem::Specification.new('picoruby-wasm-rapp') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'PicoRuby for WebAssembly'
  spec.add_dependency 'picoruby-wasm'
  spec.require_name = 'rapp'
end

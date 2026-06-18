MRuby::Gem::Specification.new('picoruby-indexeddb') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Ruby-idiomatic IndexedDB wrapper for PicoRuby.wasm'

  spec.add_dependency 'picoruby-wasm'
  spec.add_dependency 'picoruby-json'

  spec.require_name = 'indexeddb'
end

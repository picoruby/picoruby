MRuby::Gem::Specification.new('picoruby-shuttle') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Static site generator powered by PicoRuby.wasm'

  spec.add_dependency 'picoruby-markdown'
end

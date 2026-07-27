MRuby::Gem::Specification.new('picoruby-uri') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Simplified CRuby-compatible URI parsing and form encoding'
  spec.description = 'URI.parse / URI.encode_www_form(_component) for PicoRuby. Pure Ruby, usable on microcontrollers and on picoruby.wasm alike.'

  spec.require_name = 'uri'
end

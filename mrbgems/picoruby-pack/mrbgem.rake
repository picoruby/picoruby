MRuby::Gem::Specification.new('picoruby-pack') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'pack/unpack for PicoRuby (C implementation)'

  spec.test_rbfiles = [] unless build.vm_mruby? && !build.wasm?

  spec.add_conflict 'mruby-pack'
end

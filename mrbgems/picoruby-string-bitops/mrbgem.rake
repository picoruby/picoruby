MRuby::Gem::Specification.new('picoruby-string-bitops') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Bit operations for String on FemtoRuby'

  spec.add_conflict 'picoruby-mruby'
end

MRuby::Gem::Specification.new('picoruby-metaprog') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Meta Programming for PicoRuby'

  spec.add_conflict 'picoruby-mruby'

  # String#gsub used to live here; picoruby-regexp provides it now
  spec.add_dependency 'picoruby-regexp'
end

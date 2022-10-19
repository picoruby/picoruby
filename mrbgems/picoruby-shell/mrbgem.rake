MRuby::Gem::Specification.new('picoruby-shell') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'PicoRuby Shell library'
  spec.add_dependency 'picoruby-terminal'
  spec.add_dependency 'picoruby-vfs'
  spec.add_dependency 'picoruby-filesystem-fat'
end

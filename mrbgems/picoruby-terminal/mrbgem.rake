MRuby::Gem::Specification.new('picoruby-terminal') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Library for terminal-based application'
  spec.add_dependency 'picoruby-io'
  spec.add_dependency 'picoruby-filesystem-fat'
  spec.add_dependency 'picoruby-vfs'
end


MRuby::Gem::Specification.new('picoruby-editor') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Library for editor-like application'
  spec.add_dependency 'picoruby-io-console'
  spec.add_dependency 'picoruby-filesystem-fat'
  spec.add_dependency 'picoruby-vfs'
end


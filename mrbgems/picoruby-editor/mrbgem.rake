MRuby::Gem::Specification.new('picoruby-editor') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Library for editor-like application'

  if build.posix?
    spec.add_dependency 'picoruby-io'
  else
    spec.add_dependency 'picoruby-filesystem-fat'
    spec.add_dependency 'picoruby-vfs'
  end
  spec.add_dependency 'picoruby-env'
  spec.add_dependency 'picoruby-io-console'
end


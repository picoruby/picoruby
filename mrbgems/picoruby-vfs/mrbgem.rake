MRuby::Gem::Specification.new('picoruby-vfs') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Virtual-File-System-like wrapper for filesystems'

  spec.add_dependency 'picoruby-env'
  spec.add_dependency 'picoruby-time'
end

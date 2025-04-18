MRuby::Gem::Specification.new('picoruby-vfs') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Virtual-File-System-like wrapper for filesystems'

  spec.add_dependency 'picoruby-env'
  if build.vm_mrubyc?
    spec.add_dependency 'picoruby-time-class'
  elsif build.vm_mruby?
    spec.add_dependency 'mruby-time'
  end
end

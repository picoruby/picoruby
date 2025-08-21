MRuby::Gem::Specification.new('picoruby-editor') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Library for editor-like application'

  if build.posix?
    if build.vm_mrubyc?
      spec.add_dependency('picoruby-posix-io')
    elsif build.vm_mruby?
      spec.add_dependency('mruby-io')
    end
  else
    spec.add_dependency 'picoruby-filesystem-fat'
    spec.add_dependency 'picoruby-vfs'
  end
  spec.add_dependency 'picoruby-env'
  spec.add_dependency 'picoruby-io-console'
end


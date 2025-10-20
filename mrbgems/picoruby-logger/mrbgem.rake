MRuby::Gem::Specification.new('picoruby-logger') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Logger class'

  if build.vm_mruby?
    spec.add_dependency 'mruby-time'
  elsif build.vm_mrubyc?
    spec.add_dependency 'picoruby-time-class'
  end
end

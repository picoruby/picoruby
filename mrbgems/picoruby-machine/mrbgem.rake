MRuby::Gem::Specification.new('picoruby-machine') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Machine class'

  spec.add_dependency 'picoruby-io-console'

  spec.posix

  if build.posix?
    cc.defines << "PICORB_PLATFORM_POSIX"
  end
end

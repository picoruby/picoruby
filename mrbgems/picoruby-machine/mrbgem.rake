MRuby::Gem::Specification.new('picoruby-machine') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Machine class'

  spec.add_dependency 'picoruby-io-console'

  spec.posix

  if build.gems.map(&:name).include?('picoruby-mruby')
    # Workaround:
    #   Locate mruby-io at the top of gem_init.c
    #   to define IO.open earlier than this gems
    if build.posix?
      spec.add_dependency 'mruby-io'
    end
  end
end

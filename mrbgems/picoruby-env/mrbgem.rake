MRuby::Gem::Specification.new('picoruby-env') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'ENV'

  if build.gems.map(&:name).include?('picoruby-mruby')
    # Workaround:
    #   Locate picoruby-mruby at the top of gem_init.c
    #   to define Kernel#require earlier than other gems
    spec.add_dependency 'picoruby-mruby'
  end

  spec.posix
end

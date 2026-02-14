MRuby::Gem::Specification.new('picoruby-drb') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'dRuby (Distributed Ruby) for PicoRuby (mruby VM only)'

  spec.add_dependency 'picoruby-mruby' # picoruby-drb doesn't support mruby/c
  spec.add_dependency 'picoruby-marshal'

  # Socket dependency is optional
  # TCP protocol support is only available when socket is present
  unless build.cc.command =~ /emcc/
    spec.add_dependency 'picoruby-socket'
  end
end

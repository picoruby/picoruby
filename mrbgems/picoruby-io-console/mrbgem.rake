MRuby::Gem::Specification.new('picoruby-io-console') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'IO class'

  spec.add_dependency 'picoruby-env'

  spec.require_name = 'io/console'

  build.porting(dir)
end



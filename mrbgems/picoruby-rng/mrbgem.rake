MRuby::Gem::Specification.new('picoruby-rng') do |spec|
  spec.license = 'MIT'
  spec.author  = 'Ryo Kajiwara'
  spec.summary = 'Random Number Generator for PicoRuby'

  build.porting(dir)
end

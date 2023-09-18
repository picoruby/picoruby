MRuby::Gem::Specification.new('picoruby-require') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'PicoRuby require gem'
  spec.add_dependency 'picoruby-vfs'
  spec.add_dependency 'picoruby-filesystem-fat'
  spec.add_dependency 'picoruby-sandbox'
end


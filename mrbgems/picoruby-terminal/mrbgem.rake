MRuby::Gem::Specification.new('picoruby-terminal') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Library for terminal-based application'
  spec.add_dependency 'picoruby-sandbox'

  spec.hal_obj
end


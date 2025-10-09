MRuby::Gem::Specification.new('picoruby-prk-rgb') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'PRK Firmware / class RGB'

  spec.add_dependency 'picoruby-numeric-ext'

  spec.require_name = 'rgb'
end


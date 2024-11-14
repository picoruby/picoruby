MRuby::Gem::Specification.new('picoruby-prk-keyboard') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'PRK Firmware / class Keyboard'

  spec.require_name = 'keyboard'

  spec.add_dependency 'picoruby-sandbox'
  spec.add_dependency 'picoruby-picorubyvm'
end



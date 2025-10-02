MRuby::Gem::Specification.new('picoruby-uart') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'UART class / General peripherals'

  spec.add_dependency 'picoruby-gpio'
  spec.posix
end


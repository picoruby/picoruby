MRuby::Gem::Specification.new('picoruby-uart') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'UART class / General peripherals'

  spec.add_dependency 'picoruby-gpio'
  spec.add_dependency 'picoruby-machine'
  spec.cc.include_paths << "#{MRUBY_ROOT}/mrbgems/picoruby-machine/include"
end


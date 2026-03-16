MRuby::Gem::Specification.new('picoruby-network') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Network abstraction layer for different platforms'

  spec.add_dependency 'picoruby-machine'
  if %w(pico_w pico2_w).include?(ENV['PICORB_BOARD'])
    spec.add_dependency 'picoruby-cyw43'
  end
end

MRuby::Gem::Specification.new('picoruby-pio') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'PIO (Programmable I/O) class for RP2040/RP2350'

  spec.add_dependency 'picoruby-gpio'
end

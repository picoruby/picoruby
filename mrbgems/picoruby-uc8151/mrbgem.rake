MRuby::Gem::Specification.new('picoruby-uc8151') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'UC8151 e-ink display driver using SPI'

  spec.add_dependency 'picoruby-spi'
  spec.add_dependency 'picoruby-gpio'
  spec.add_dependency 'picoruby-vram'
  spec.add_dependency 'picoruby-bdffont'
end

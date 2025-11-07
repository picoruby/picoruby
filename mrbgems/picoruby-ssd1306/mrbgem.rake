MRuby::Gem::Specification.new('picoruby-ssd1306') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'SSD1306 OLED display driver using I2C'

  spec.add_dependency 'picoruby-i2c'
  spec.add_dependency 'picoruby-vram'
  spec.add_dependency 'picoruby-terminus'
end


MRuby::Gem::Specification.new('picoruby-adafruit_pcf8523') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Adafruit PCF8523 I2C RTC module'

  spec.add_dependency 'picoruby-i2c'
  spec.add_dependency 'picoruby-time'
end


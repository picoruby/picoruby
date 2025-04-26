MRuby::Gem::Specification.new('picoruby-adafruit_pcf8523') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Adafruit PCF8523 I2C RTC module'

  spec.add_dependency 'picoruby-i2c'
  if build.vm_mrubyc?
    spec.add_dependency 'picoruby-time-class'
  elsif build.vm_mruby?
    spec.add_dependency 'mruby-time'
  end
end


MRuby::Gem::Specification.new('picoruby-aht25') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'AHT25 Humidity and Temperature Module https://akizukidenshi.com/catalog/g/gM-16731/'

  spec.add_dependency 'picoruby-i2c'
end


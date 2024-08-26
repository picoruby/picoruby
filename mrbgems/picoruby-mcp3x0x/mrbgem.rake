MRuby::Gem::Specification.new('picoruby-mcp3x0x') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'MCP3x0x ADC module (SPI interface)'

  spec.add_dependency 'picoruby-machine'
  spec.add_dependency 'picoruby-spi'
end


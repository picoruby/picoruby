MRuby::Gem::Specification.new('picoruby-mcp3204') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'MCP3204 ADC module'

  spec.add_dependency 'picoruby-machine'
  spec.add_dependency 'picoruby-spi'
end


MRuby::Gem::Specification.new('picoruby-mcp3424') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'MCP3424 ADC module'

  spec.add_dependency 'picoruby-machine'
  spec.add_dependency 'picoruby-i2c'
end


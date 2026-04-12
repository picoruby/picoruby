MRuby::Gem::Specification.new('picoruby-irq') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'IRQ module'

  spec.add_dependency 'picoruby-gpio'
end

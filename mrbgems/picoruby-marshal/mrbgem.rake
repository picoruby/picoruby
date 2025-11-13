MRuby::Gem::Specification.new('picoruby-marshal') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Marshal for PicoRuby (for dRuby support)'

  spec.add_dependency 'picoruby-pack'

  # Add test files
  spec.test_rbfiles = Dir.glob("#{dir}/test/*.rb")
end

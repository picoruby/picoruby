MRuby::Gem::Specification.new('picoruby-pack') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'pack/unpack for PicoRuby (C implementation)'

  # Add test files
  spec.test_rbfiles = Dir.glob("#{dir}/test/*.rb")
end


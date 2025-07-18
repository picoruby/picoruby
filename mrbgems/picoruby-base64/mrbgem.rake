MRuby::Gem::Specification.new('picoruby-base64') do |spec|
  spec.license = 'MIT'
  spec.author  = 'Ryo Kajiwara'
  spec.summary = 'Base64 for PicoRuby'
  spec.test_rbfiles = Dir.glob("#{spec.dir}/test/*.rb")
end

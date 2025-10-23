MRuby::Gem::Specification.new('picoruby-sandbox') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Sandbox class for shell and picoirb'

  spec.add_dependency 'picoruby-io-console'
  if build.vm_mrubyc?
    spec.add_dependency 'picoruby-metaprog'
  end
end


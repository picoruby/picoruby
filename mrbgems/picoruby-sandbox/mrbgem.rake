MRuby::Gem::Specification.new('picoruby-sandbox') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Sandbox class for shell and picoirb'

  if build.vm_mrubyc?
    spec.add_dependency 'picoruby-metaprog'
  end
end


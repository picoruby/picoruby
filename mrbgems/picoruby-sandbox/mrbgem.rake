MRuby::Gem::Specification.new('picoruby-sandbox') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Sandbox class for shell and picoirb'

  spec.add_dependency 'picoruby-io-console'
  if spec.cc.defines.include?("PICORB_VM_MRUBYC")
    spec.add_dependency 'picoruby-metaprog'
  elsif spec.cc.defines.include?("PICORB_VM_MRUBY")
    spec.cc.include_paths << "#{build.gems['picoruby-mruby'].dir}/include"
  end
end


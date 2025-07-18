MRuby::Gem::Specification.new('picoruby-eval') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Kernel#eval implementation for PicoRuby'

  if build.vm_mruby?
    spec.cc.include_paths << "#{build.gems['picoruby-mruby'].dir}/include"
  end
end
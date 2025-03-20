MRuby::Gem::Specification.new('picoruby-picorubyvm') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'PicoRubyVM class'

  if build.vm_mruby?
    cc.include_paths << "#{build.gems['picoruby-mruby'].dir}/include"
  end
end

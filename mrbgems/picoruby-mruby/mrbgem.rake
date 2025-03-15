MRuby::Gem::Specification.new('picoruby-mruby') do |spec|
  spec.license = 'MIT'
  spec.authors = 'HASUMI Hitoshi'
  spec.summary = 'mruby library'

  spec.add_conflict 'picoruby-mrubyc'

  spec.cc.include_paths << "#{build.gems['mruby-compiler2'].dir}/include"
  spec.cc.include_paths << "#{build.gems['mruby-compiler2'].dir}/lib/prism/include"

  dir_for_enhanced_rule = "lib/mruby/src"
  Dir.glob(File.join(dir, "#{dir_for_enhanced_rule}/*.c")).each do |file|
    obj = objfile(file.pathmap("#{build_dir}/#{dir_for_enhanced_rule}/%n"))
    build.libmruby_objs << obj
    file obj => [file] do |t|
      cc.run t.name, t.prerequisites.first
    end
  end

  if build.posix?
    cc.defines << "PICORB_PLATFORM_POSIX"
  end
end

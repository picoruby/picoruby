MRuby::Gem::Specification.new('picoruby-mruby') do |spec|
  spec.license = ['MIT', 'Apache-2.0', 'BSD-3-Clause', 'BSD']
  spec.authors = 'HASUMI Hitoshi'
  spec.summary = 'mruby library'

  spec.add_conflict 'picoruby-mrubyc'

  spec.cc.include_paths << "#{build.gems['mruby-compiler2'].dir}/include"
  spec.cc.include_paths << "#{build.gems['mruby-compiler2'].dir}/lib/prism/include"

  alloc_dir = if spec.cc.defines.include?("PICORB_ALLOC_TINYALLOC")
    "#{dir}/lib/tinyalloc"
  elsif spec.cc.defines.include?("PICORB_ALLOC_O1HEAP")
    "#{dir}/lib/o1heap/o1heap"
  elsif spec.cc.defines.include?("PICORB_ALLOC_TLSF")
    "#{dir}/lib/tlsf"
  end
  if alloc_dir
    Dir.glob("#{alloc_dir}/*.c").each do |src|
      obj = objfile(src.pathmap("#{build_dir}/allocator/%n"))
      build.libmruby_objs << obj
      file obj => src do |t|
        cc.run t.name, t.prerequisites.first
      end
    end
  end

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
  if cc.defines.any?{ _1.start_with?("PICORUBY_DEBUG=1") }
    cc.defines << "_DEBUG" # for tlsf.c
  end
end

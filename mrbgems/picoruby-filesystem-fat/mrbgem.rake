MRuby::Gem::Specification.new('picoruby-filesystem-fat') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'FAT filesystem'

  if build.vm_mrubyc?
    spec.add_dependency 'picoruby-time-class'
  elsif build.vm_mruby?
    spec.add_dependency 'mruby-time'
  end

  # TODO: use #porting instead
  Dir.glob("#{dir}/src/hal/*.c").each do |src|
    obj = "#{build_dir}/src/#{objfile(File.basename(src, ".c"))}"
    file obj => src do |t|
      cc.run(t.name, t.prerequisites[0])
    end
    objs << obj
  end

  Dir.glob("#{dir}/lib/ff14b/source/*.c").each do |src|
    obj = "#{build_dir}/src/#{objfile(File.basename(src, ".c"))}"
    file obj => [src, "#{dir}/lib/ff14b/source/ffconf.h"] do |t|
      spec.cc.run t.name, t.prerequisites[0]
    end
    spec.objs << obj
  end
end


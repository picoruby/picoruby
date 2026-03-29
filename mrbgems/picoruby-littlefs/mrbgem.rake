MRuby::Gem::Specification.new('picoruby-littlefs') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'Filesystem using littlefs'

  spec.add_dependency 'picoruby-time'

  %w[lfs.c lfs_util.c].each do |src_name|
    src = "#{dir}/lib/littlefs/#{src_name}"
    obj = "#{build_dir}/src/#{objfile(File.basename(src_name, ".c"))}"
    file obj => src do |t|
      spec.cc.run t.name, t.prerequisites[0]
    end
    spec.objs << obj
  end

  if build.name == "nrf52"
    src = "#{dir}/ports/nrf52/flash_hal.c"
    obj = src.relative_path_from(dir).pathmap("#{build_dir}/%X.o")
    spec.objs << obj
  end
end

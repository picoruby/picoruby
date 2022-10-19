MRuby::Gem::Specification.new('picoruby-filesystem-fat') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'FAT filesystem'
  spec.add_dependency 'picoruby-vfs', core: 'picoruby-vfs'

  obj = "#{build_dir}/src/#{objfile("discio")}"
  file obj => "#{dir}/src/hal/diskio.c" do |t|
    spec.cc.run(t.name, t.prerequisites[0])
  end
  spec.objs << obj

  hal = cc.defines.find { |d| d.start_with?("MRBC_USE_HAL_") }
  if hal
    Dir.glob("#{dir}/src/hal/#{hal.sub("MRBC_USE_HAL_", "").downcase}/*.c").each do |src|
      obj = "#{build_dir}/src/#{objfile(File.basename(src, ".c"))}"
      file obj => src do |t|
        spec.cc.run(t.name, t.prerequisites[0])
      end
      spec.objs << obj
    end
  end

  Dir.glob("#{dir}/lib/ff14b/source/*.c").each do |src|
    obj = "#{build_dir}/src/#{objfile(File.basename(src, ".c"))}"
    file obj => src do |t|
      spec.cc.run(t.name, t.prerequisites[0])
    end
    spec.objs << obj
  end
end


MRuby::Gem::Specification.new 'picoruby-bin-r2p2' do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'R2P2 executable for POSIX'

  spec.add_dependency 'picoruby-shell'

  if build.vm_mruby?
    spec.add_dependency 'picoruby-mruby'
    build.cc.include_paths << "#{build.gems['picoruby-mruby'].dir}/include"
  elsif build.vm_mrubyc?
    spec.add_dependency 'picoruby-mrubyc'
  end

  spec.cc.defines << "MRBC_USE_HAL_POSIX"

  app_dir = "#{build_dir}/tools/mrblib"
  build.cc.include_paths << app_dir
  spec.cc.include_paths << app_dir
  directory app_dir

  app_rb = "#{dir}/tools/mrblib/app.rb"
  app_src = "#{app_dir}/app.c"
  file app_src => [app_dir, app_rb] do |t|
    File.open(t.name, "w") do |f|
      mrbc.run f, app_rb, "app", cdump: false
    end
  end
  app_obj = objfile(app_src.pathmap("#{app_dir}/%n"))
  build.libmruby_objs << app_obj

  exec = exefile("#{build.build_dir}/bin/r2p2")
  r2p2_src = "#{dir}/tools/r2p2/r2p2.c"
  r2p2_obj = objfile(r2p2_src.pathmap("#{build_dir}/tools/r2p2/%n"))
  task r2p2_obj => [app_src, r2p2_src] do |t|
    build.cc.run t.name, r2p2_src
  end
  file exec => [r2p2_obj,  build.libmruby_static] do |f|
    build.linker.run f.name, f.prerequisites
  end
  build.bins << 'r2p2'

end

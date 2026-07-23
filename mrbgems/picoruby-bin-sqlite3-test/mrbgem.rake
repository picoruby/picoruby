MRuby::Gem::Specification.new('picoruby-bin-sqlite3-test') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'SQLite3 gem test binary'

  spec.add_conflict 'picoruby-mrubyc'
  spec.add_dependency 'picoruby-sqlite3'
  spec.add_dependency 'picoruby-littlefs'
  spec.add_dependency 'picoruby-require'
  spec.add_dependency 'picoruby-picorubyvm'
  spec.linker.libraries << 'm'

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

  exec = exefile("#{build.build_dir}/bin/sqlite3-test")
  src = "#{dir}/tools/sqlite3-test/sqlite3-test.c"
  obj = objfile(src.pathmap("#{build_dir}/tools/sqlite3-test/%n"))
  task obj => [app_src, src] do |t|
    build.cc.run t.name, src
  end
  file exec => [obj, build.libmruby_static] do |f|
    build.linker.run f.name, f.prerequisites
  end
  build.bins << 'sqlite3-test'

end

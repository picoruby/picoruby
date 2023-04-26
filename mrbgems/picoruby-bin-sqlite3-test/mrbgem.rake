MRuby::Gem::Specification.new('picoruby-bin-sqlite3-test') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'SQLite3 gem test binary'

  spec.add_dependency 'picoruby-sqlite3'
  spec.add_dependency 'picoruby-shell'
  spec.linker.libraries << 'm'

  #mrubyc_objs = Rake::Task.tasks.select{ |t|
  #  t.name.match? /picoruby-mrubyc.+\.o\z/
  #}.map(&:name)

  #test_obj = "#{build_dir}/sqlite3-test/#{objfile("sqlite3-test")}"

  #app_c = "#{build_dir}/mrblib/app.c"
  #file app_c => "#{dir}/sqlite3-test/app.rb" do |t|
  #  File.open(t.name, 'w') do |out|
  #    spec.mrbc.run out, t.prerequisites[0], "app", false
  #  end
  #end

  #file test_obj => ["#{dir}/sqlite3-test/sqlite3-test.c", app_c] do |t|
  #  spec.cc.run t.name, t.prerequisites[0], [], [File.dirname(app_c)]
  #end

  #exec = exefile("#{build.build_dir}/bin/sqlite3-test")
  ##file exec => [test_obj] + mrubyc_objs do |t|
  #file exec => [test_obj] + [build.libmruby_static] do |t|
  #  spec.linker.run t.name, t.prerequisites
  #end

  spec.cc.defines << "MRBC_USE_HAL_POSIX"

  exec = exefile("#{build.build_dir}/bin/sqlite3-test")
  sqlite3_test_objs = Dir.glob("#{spec.dir}/tools/sqlite3-test/*.c").map do |f|
    objfile(f.pathmap("#{spec.build_dir}/tools/sqlite3-test/%n"))
  end
  file exec => sqlite3_test_objs << build.libmruby_static do |f|
    build.linker.run f.name, f.prerequisites
  end
  build.bins << 'sqlite3-test'
end

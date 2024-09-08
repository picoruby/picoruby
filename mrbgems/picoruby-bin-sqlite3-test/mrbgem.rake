MRuby::Gem::Specification.new('picoruby-bin-sqlite3-test') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'SQLite3 gem test binary'

  spec.add_dependency 'picoruby-sqlite3'
  spec.add_dependency 'picoruby-shell'
  spec.linker.libraries << 'm'

  spec.cc.defines << "MRBC_USE_HAL_POSIX"
  hal_src = "#{build.gems['picoruby-mrubyc'].dir}/repos/mrubyc/hal/posix/hal.c"
  hal_obj = objfile(hal_src.pathmap("#{build.gems['picoruby-mrubyc'].build_dir}/src/%n"))
  file hal_obj => hal_src do |f|
    cc.run f.name, f.prerequisites.first
  end

  exec = exefile("#{build.build_dir}/bin/sqlite3-test")
  sqlite3_test_objs = Dir.glob("#{spec.dir}/tools/sqlite3-test/*.c").map do |f|
    objfile(f.pathmap("#{spec.build_dir}/tools/sqlite3-test/%n"))
  end
  file exec => sqlite3_test_objs << build.libmruby_static << hal_obj do |f|
    build.linker.run f.name, f.prerequisites
  end
  build.bins << 'sqlite3-test'

end

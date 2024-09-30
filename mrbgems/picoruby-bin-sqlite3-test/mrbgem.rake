MRuby::Gem::Specification.new('picoruby-bin-sqlite3-test') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'SQLite3 gem test binary'

  spec.add_dependency 'picoruby-sqlite3'
  spec.add_dependency 'picoruby-shell'
  spec.add_dependency 'picoruby-require'
  spec.add_dependency 'picoruby-picorubyvm'
  spec.linker.libraries << 'm'

  spec.cc.include_paths << "#{build_dir}/../"

  spec.cc.defines << "MRBC_USE_HAL_POSIX"

  src = "#{spec.dir}/tools/sqlite3-test/sqlite3-test.c"
  obj = objfile(src.pathmap("#{spec.build_dir}/tools/sqlite3-test/%n"))
  exec = exefile("#{build.build_dir}/bin/sqlite3-test")
  file exec => [obj] << build.libmruby_static do |f|
    build.linker.run f.name, f.prerequisites
  end
  build.bins << 'sqlite3-test'

end

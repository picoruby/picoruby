MRuby::Gem::Specification.new('picoruby-sqlite3-class') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'SQLite3'

  spec.require_name = 'sqlite3'

  spec.add_dependency 'picoruby-mrubyc'
  spec.add_dependency 'picoruby-filesystem-fat'
  spec.add_dependency 'picoruby-time-class'

  # SQLite build configuration https://sqlite.org/compile.html
  spec.cc.defines << "SQLITE_OS_OTHER=1" # You need to implement sqlite3_os_init() and sqlite3_os_end()
  spec.cc.defines << "SQLITE_OMIT_ANALYZE=1"
  # spec.cc.defines << "SQLITE_OMIT_ATTACH=1"
  spec.cc.defines << "SQLITE_OMIT_AUTHORIZATION=1"
  spec.cc.defines << "SQLITE_OMIT_AUTOINIT=1"
  # spec.cc.defines << "SQLITE_OMIT_DECLTYPE=1"
  spec.cc.defines << "SQLITE_OMIT_DEPRECATED=1"
  spec.cc.defines << "SQLITE_OMIT_EXPLAIN=1"
  spec.cc.defines << "SQLITE_OMIT_LOAD_EXTENSION=1"
  spec.cc.defines << "SQLITE_OMIT_LOCALTIME=1"
  spec.cc.defines << "SQLITE_OMIT_LOOKASIDE=1"
  spec.cc.defines << "SQLITE_OMIT_PROGRESS_CALLBACK=1"
  spec.cc.defines << "SQLITE_OMIT_QUICKBALANCE=1"
  spec.cc.defines << "SQLITE_OMIT_SHARED_CACHE=1"
  spec.cc.defines << "SQLITE_OMIT_TCL_VARIABLE=1"
  spec.cc.defines << "SQLITE_OMIT_UTF16=1"
  # spec.cc.defines << "SQLITE_OMIT_WAL=1"
  spec.cc.defines << "SQLITE_UNTESTABLE=1"
  spec.cc.defines << "SQLITE_DEFAULT_SYNCHRONOUS=2"
  spec.cc.defines << "SQLITE_DEFAULT_WAL_SYNCHRONOUS=1"
  spec.cc.defines << "SQLITE_THREADSAFE=0"
  spec.cc.defines << "SQLITE_LIKE_DOESNT_MATCH_BLOBS=1"
  spec.cc.defines << "SQLITE_MAX_EXPR_DEPTH=0"
  spec.cc.defines << "SQLITE_DEFAULT_LOCKING_MODE=1"
  # spec.cc.defines << "SQLITE_ENABLE_RTREE=0"
  spec.cc.defines << "SQLITE_ENABLE_MEMORY_MANAGEMENT=1"
  spec.cc.defines << "SQLITE_DEFAULT_MEMSTATUS=0"
  spec.cc.defines << "SQLITE_ZERO_MALLOC=1"

#  spec.cc.defines << "SQLITE_OMIT_VIRTUALTABLE=1"

  obj = "#{build_dir}/src/#{objfile("sqlite3")}"
  file obj => "#{dir}/lib/sqlite-amalgamation-3410100/sqlite3.c" do |t|
    spec.cc.run t.name, t.prerequisites[0]
  end
  spec.objs << obj

  ##
  # test binary

  spec.add_dependency 'picoruby-shell'
  spec.linker.libraries << 'm'

  mrubyc_objs = Rake::Task.tasks.select{ |t|
    t.name.match? /picoruby-mrubyc.+\.o\z/
  }.map(&:name)

  test_obj = "#{build_dir}/sqlite3-test/#{objfile("sqlite3-test")}"

  ruby_c = "#{dir}/sqlite3-test/ruby.c"
  file ruby_c => "#{dir}/sqlite3-test/ruby.rb" do |t|
    File.open(t.name, 'w') do |out|
      spec.mrbc.run out, t.prerequisites[0], "ruby", false
    end
  end

  file test_obj => ["#{dir}/sqlite3-test/sqlite3-test.c", ruby_c] do |t|
    spec.cc.run t.name, t.prerequisites[0]
  end

  exec = exefile("#{build.build_dir}/bin/sqlite3-test")
  #file exec => [test_obj] + mrubyc_objs do |t|
  file exec => [test_obj] + [build.libmruby_static] do |t|
    spec.linker.run t.name, t.prerequisites
  end

  build.bins << 'sqlite3-test'
end



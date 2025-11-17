MRuby::Gem::Specification.new('picoruby-sqlite3') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'SQLite3'

  spec.add_dependency 'picoruby-mrubyc'
  spec.add_dependency 'picoruby-vfs'
  spec.add_dependency 'picoruby-time'

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
  spec.cc.defines << "SQLITE_OMIT_LOCALTIME=0"
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
    spec.cc.run(
      t.name,
      t.prerequisites[0],
      [], # _defines
      [], # _include_paths
      [   # _flags
        "-Wno-undef",
        "-Wno-discarded-qualifiers",
        "-Wno-unused-function",
        "-Wno-unused-variable",
        "-Wno-unused-value",
        "-Wno-unused-but-set-variable"
      ]
    )
  end
  spec.objs << obj

end


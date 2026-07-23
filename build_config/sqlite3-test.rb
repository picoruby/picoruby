# Host build that exercises picoruby-sqlite3 on top of picoruby-littlefs.
# littlefs uses its ports/posix RAM block device here, so the whole stack runs
# without a board attached. Note this is a plain Build, not a CrossBuild:
# ports/ are only picked up by a native build.
MRuby::Build.new("sqlite3-test") do |conf|

  conf.toolchain :gcc

  conf.cc.defines << "MRB_TICK_UNIT=4"
  conf.cc.defines << "MRB_TIMESLICE_TICK_COUNT=3"

  conf.cc.defines << "PICORB_PLATFORM_POSIX"

  conf.cc.defines << "MRB_INT64"
  conf.cc.defines << "MRB_NO_BOXING"
  conf.cc.defines << "MRB_UTF8_STRING"

  conf.picoruby

  # core.gembox would pull in picoruby-posix-io and picoruby-dir on a POSIX
  # host, and both conflict with picoruby-littlefs, so the gems are named here
  conf.gem core: "mruby-compiler"
  conf.gem core: "mruby-bin-mrbc"

  # The subset of stdlib that picoruby-vfs and picoruby-sqlite3 need
  %w[mruby-string-ext mruby-array-ext mruby-hash-ext mruby-sprintf].each do |gem_name|
    conf.gem gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/#{gem_name}"
  end

  conf.gem core: "picoruby-littlefs"
  conf.gem core: "picoruby-bin-sqlite3-test"
end

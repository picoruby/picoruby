MRuby::CrossBuild.new("no-libc-host") do |conf|

  conf.toolchain

  conf.cc.host_command = "gcc"
  conf.cc.command = "gcc"

  conf.cc.defines << "PICORUBY_INT64"
  conf.cc.defines << "MRBC_USE_MATH"
  conf.cc.defines << "MRBC_USE_HAL_POSIX"

  conf.gem core: "picoruby-bin-sqlite3-test"

  conf.picoruby(alloc_libc: true)
end


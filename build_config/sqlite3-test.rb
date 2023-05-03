MRuby::CrossBuild.new("no-libc-host") do |conf|

  conf.toolchain

  conf.picoruby

  conf.cc.host_command = "gcc"
  conf.cc.command = "gcc"

  conf.cc.flags.delete "-O3"
  conf.cc.flags << "-O0"
  conf.cc.flags << "-g"
  conf.cc.flags << "-fno-inline"

  conf.cc.defines << "MRBC_INT64"
  conf.cc.defines << "MRBC_USE_MATH"
  conf.cc.defines << "MRBC_USE_HAL_POSIX"

  conf.gem core: "picoruby-bin-sqlite3-test"
end


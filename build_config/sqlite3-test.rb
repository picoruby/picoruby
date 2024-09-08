MRuby::CrossBuild.new("no-libc-host") do |conf|

  conf.toolchain

  conf.picoruby

  conf.cc.host_command = "gcc"
  conf.cc.command = "gcc"

  conf.cc.defines << 'MRC_CUSTOM_ALLOC'
  conf.cc.defines << "MRBC_INT64"
  conf.cc.defines << "MRBC_USE_MATH"
  conf.cc.defines << "MRBC_USE_HAL_POSIX"

  conf.gem core: "picoruby-bin-sqlite3-test"
end


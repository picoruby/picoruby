MRuby::CrossBuild.new("no-libc-host") do |conf|

  conf.toolchain

  conf.cc.host_command = "gcc"
  conf.cc.command = "gcc"

  conf.cc.defines << "PICORB_INT64"
  conf.cc.defines << "MRBC_USE_MATH"
  conf.cc.defines << "PICORB_PLATFORM_POSIX"

  conf.femtoruby

  conf.gem core: "picoruby-bin-sqlite3-test"
end

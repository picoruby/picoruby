MRuby::CrossBuild.new("no-libc-host") do |conf|

  conf.toolchain

  conf.picoruby

  conf.cc.flags.delete "-O3"
  conf.cc.flags << "-O0"
  conf.cc.flags << "-g"
  conf.cc.flags << "-fno-inline"

  conf.cc.defines << "LEMON_PICORBC=1"
  conf.cc.defines << "MRBC_INT64"
  conf.cc.defines << "MRBC_USE_HAL_POSIX"

  conf.gembox "r2p2"
  conf.gem core: "picoruby-bin-r2p2"
  conf.gem core: "picoruby-mbedtls"

end


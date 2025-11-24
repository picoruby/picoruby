MRuby::Build.new do |conf|

  conf.toolchain

  conf.cc.defines << "PICORUBY_INT64"
  conf.cc.defines << "MRBC_TICK_UNIT=4"
  conf.cc.defines << "MRBC_TIMESLICE_TICK_COUNT=3"

  conf.posix
  conf.picoruby(alloc_libc: true)

  conf.gembox "minimum"
  conf.gembox "core"
  conf.gem core: "picoruby-picotest"
  conf.gem core: "picoruby-metaprog"
  conf.gem core: "picoruby-pack"
  conf.gem core: "picoruby-time"
  conf.gem core: "picoruby-mbedtls"
  conf.gem core: "picoruby-jwt"
  conf.gem core: "picoruby-net"
end

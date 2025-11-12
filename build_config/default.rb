MRuby::Build.new do |conf|

  conf.toolchain

  conf.cc.defines << "PICORUBY_INT64"
  conf.cc.defines << "MRBC_TICK_UNIT=4"
  conf.cc.defines << "MRBC_TIMESLICE_TICK_COUNT=3"

  conf.posix
  conf.picoruby(alloc_libc: false)

  conf.gembox "minimum"
  conf.gembox "core"
  conf.gembox "stdlib"
  conf.gembox "shell"
  conf.gem core: "picoruby-shinonome"
  conf.gem core: "picoruby-bin-r2p2"
  conf.gem core: "picoruby-socket"
end


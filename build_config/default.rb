MRuby::Build.new do |conf|

  conf.toolchain

  conf.cc.defines << "PICORUBY_INT64"
  conf.cc.defines << "MRBC_TICK_UNIT=4"
  conf.cc.defines << "MRBC_TIMESLICE_TICK_COUNT=3"

  conf.posix
  conf.picoruby(alloc_libc: false)

  # Link OpenSSL libraries for socket SSL support
  conf.linker.libraries << 'ssl'
  conf.linker.libraries << 'crypto'

  conf.gembox "minimum"
  conf.gembox "core"
  conf.gembox "stdlib"
  conf.gembox "shell"
  conf.gembox "networking"
  conf.gem core: "picoruby-shinonome"
  conf.gem core: "picoruby-bin-r2p2"
end


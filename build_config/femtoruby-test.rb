MRuby::Build.new do |conf|

  conf.toolchain

  conf.cc.defines << "PICORB_PLATFORM_POSIX"
  conf.cc.defines << "PICORB_INT64"
  conf.cc.defines << "MRBC_TICK_UNIT=4"
  conf.cc.defines << "MRBC_TIMESLICE_TICK_COUNT=3"
  conf.cc.defines << "MRBC_USE_STRING_UTF8"

  conf.femtoruby(alloc_libc: true)

  # Link OpenSSL libraries for socket SSL support
  conf.linker.libraries << 'ssl'
  conf.linker.libraries << 'crypto'

  conf.gembox "minimum"
  conf.gembox "core"
  conf.gem core: "picoruby-picotest"
  conf.gem core: "picoruby-metaprog"
  conf.gem core: "picoruby-pack"
end

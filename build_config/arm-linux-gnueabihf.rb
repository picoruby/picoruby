MRuby::CrossBuild.new("arm-linux-gnueabihf") do |conf|

  conf.toolchain :gcc

  conf.cc.command = 'arm-linux-gnueabihf-gcc'
  conf.cc.flags << '-mcpu=cortex-a7'
  conf.cc.flags << '-D_FILE_OFFSET_BITS=64'
  conf.cc.flags << '-D_LARGEFILE64_SOURCE'
  conf.cc.flags << '-D_GNU_SOURCE'
  conf.linker.command = 'arm-linux-gnueabihf-gcc'
  conf.linker.libraries = %w(m c gcc resolv)
  conf.linker.flags_after_libraries << '-ldl'

  conf.linker.flags << '-Wl,-rpath,/usr/arm-linux-gnueabihf/lib'
  conf.archiver.command = 'arm-linux-gnueabihf-ar'

  conf.cc.defines << "MRBC_REQUIRE_32BIT_ALIGNMENT=1"

  conf.cc.defines << "MRBC_NO_STDIO"
  conf.cc.defines << "PICORUBY_INT64"
  conf.cc.defines << "MRBC_TICK_UNIT=4"
  conf.cc.defines << "MRBC_TIMESLICE_TICK_COUNT=3"
  conf.cc.defines << "MRBC_USE_STRING_UTF8"

  conf.posix
  conf.picoruby(alloc_libc: true)

  conf.gembox "minimum"
  conf.gembox "core"
  conf.gem core: "picoruby-picotest"
  conf.gem core: "picoruby-metaprog"
  conf.gem core: "picoruby-pack"
end

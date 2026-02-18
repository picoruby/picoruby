MRuby::CrossBuild.new("mips-linux-gnu") do |conf|

  conf.toolchain :gcc

  conf.cc.command = 'mips-linux-gnu-gcc'
  conf.cc.flags << '-mips32r2'
  conf.cc.flags << '-EB' # big endian
  conf.cc.flags << '-mhard-float'
  conf.cc.flags << '-mabi=32' # o32. Most popular ABI for mips32
  conf.cc.flags << '-D_FILE_OFFSET_BITS=64'
  conf.cc.flags << '-D_LARGEFILE64_SOURCE'
  conf.cc.flags << '-D_GNU_SOURCE'
  conf.linker.command = 'mips-linux-gnu-gcc'
  conf.linker.libraries = %w(m c gcc resolv)

  conf.linker.flags << '-Wl,-rpath,/usr/mips-linux-gnu/lib'
  conf.archiver.command = 'mips-linux-gnu-ar'

  conf.cc.defines << "MRBC_REQUIRE_32BIT_ALIGNMENT=1"

  conf.cc.defines << "MRBC_BIG_ENDIAN"
  conf.cc.defines << "MRBC_NO_STDIO"
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

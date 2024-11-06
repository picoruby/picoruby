MRuby::CrossBuild.new("mips-linux-gnu") do |conf|

  conf.toolchain :gcc

  conf.cc.command = 'mips-linux-gnu-gcc'
  conf.cc.flags << '-mips32'
  conf.cc.flags << '-EB' # big endian
  conf.cc.flags << '-mhard-float'
  conf.cc.flags << '-mabi=32' # o32. Most popular ABI for mips32
  conf.cc.flags << '-D_FILE_OFFSET_BITS=64'
  conf.cc.flags << '-D_LARGEFILE64_SOURCE'
  conf.cc.flags << '-D_GNU_SOURCE'
  conf.linker.command = 'mips-linux-gnu-gcc'
  conf.linker.libraries = %w(m c gcc resolv)
#  conf.linker.flags_after_libraries = '-lssl -lcrypto -ldl'

  conf.linker.flags << '-Wl,-rpath,/usr/mips-linux-gnu/lib'
  conf.archiver.command = 'mips-linux-gnu-ar'

  conf.cc.defines << "PICORUBY_PLATFORM=posix"
  conf.cc.defines << "MRBC_REQUIRE_32BIT_ALIGNMENT=1"

  if ENV['PICORUBY_NO_LIBC_ALLOC']
    conf.cc.defines << "MRBC_USE_ALLOC_PROF"
    conf.cc.defines << 'MRC_CUSTOM_ALLOC'
  else
    conf.cc.defines << "MRBC_ALLOC_LIBC"
  end

  conf.cc.defines << "MRBC_BIG_ENDIAN"
  conf.cc.defines << "MRC_INT64"
  conf.cc.defines << "MRBC_NO_STDIO"
  conf.cc.defines << "MRBC_USE_HAL_POSIX"

  conf.gembox "minimum"
  conf.gembox "posix"
  conf.gembox "stdlib"
  conf.gembox "utils"
  # Net::NTTPSClient needs -lssl -lcrypto
#  conf.gem core: "picoruby-net"

  conf.picoruby
end

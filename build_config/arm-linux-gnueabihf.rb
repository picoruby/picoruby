MRuby::CrossBuild.new("arm-linux-gnueabihf") do |conf|

  conf.toolchain :gcc

  conf.cc.command = 'arm-linux-gnueabihf-gcc'
  conf.cc.flags << '-mcpu=cortex-a7'
  conf.cc.flags << '-D_FILE_OFFSET_BITS=64'
  conf.cc.flags << '-D_LARGEFILE64_SOURCE'
  conf.cc.flags << '-D_GNU_SOURCE'
  conf.linker.command = 'arm-linux-gnueabihf-gcc'
  conf.linker.libraries = %w(m c gcc resolv)
  conf.linker.flags << '-Wl,-rpath,/usr/arm-linux-gnueabihf/lib'
  conf.archiver.command = 'arm-linux-gnueabihf-ar'

  conf.cc.defines << "PICORUBY_POSIX"
  conf.cc.defines << "MRBC_REQUIRE_32BIT_ALIGNMENT=1"

  if ENV['PICORUBY_NO_LIBC_ALLOC']
    conf.cc.defines << "MRBC_USE_ALLOC_PROF"
    conf.cc.defines << 'MRC_CUSTOM_ALLOC'
  else
    conf.cc.defines << "MRBC_ALLOC_LIBC"
  end

  conf.cc.defines << "MRBC_NO_OBJECT_STDOUT"
  conf.cc.defines << "MRC_INT64"
  conf.cc.defines << "MRBC_USE_HAL_POSIX"

  conf.gembox "minimum"
  conf.gembox "posix"
  conf.gembox "stdlib"
  conf.gembox "utils"

  conf.linker.flags_after_libraries << '-lssl'
  conf.linker.flags_after_libraries << '-lcrypto'
  conf.linker.flags_after_libraries << '-ldl'

  conf.picoruby

end



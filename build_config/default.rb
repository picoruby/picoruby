MRuby::Build.new do |conf|

  conf.toolchain

  conf.cc.defines << "PICORUBY_PLATFORM=posix"

  if ENV['PICORUBY_NO_LIBC_ALLOC']
    conf.cc.defines << "MRBC_USE_ALLOC_PROF"
    conf.cc.defines << 'MRC_CUSTOM_ALLOC'
  else
    conf.cc.defines << "MRBC_ALLOC_LIBC"
  end

  conf.cc.defines << "MRBC_NO_STDIO"
  conf.cc.defines << "PICORUBY_INT64"
  conf.cc.defines << "MRBC_USE_HAL_POSIX"

  conf.gembox "minimum"
  conf.gembox "posix"
  conf.gembox "stdlib"
  conf.gembox "utils"
  # Net::NTTPSClient needs -lssl -lcrypto
  conf.gem core: "picoruby-net"

  conf.linker.flags_after_libraries << '-lcrypto'
  conf.linker.flags_after_libraries << '-lssl'

  conf.picoruby

end


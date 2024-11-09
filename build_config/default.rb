MRuby::Build.new do |conf|

  conf.toolchain

  conf.cc.defines << "PICORUBY_PLATFORM=posix"

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
  # Net::NTTPSClient needs -lssl -lcrypto
  conf.gem core: "picoruby-net"

  conf.picoruby

end


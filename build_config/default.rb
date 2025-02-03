MRuby::Build.new do |conf|

  conf.toolchain

  conf.cc.defines << "PICORUBY_VM_MRUBYC"
  conf.cc.defines << "PICORUBY_PLATFORM=posix"

  conf.cc.defines << "MRBC_NO_STDIO"
  conf.cc.defines << "PICORUBY_INT64"
  conf.cc.defines << "MRBC_USE_HAL_POSIX"

  conf.gembox "minimum"
  conf.gembox "posix"
  conf.gembox "stdlib"
  conf.gembox "utils"
  # Net::NTTPSClient needs -lssl -lcrypto
  conf.gem core: "picoruby-net"

  conf.picoruby(alloc_libc: true)

end


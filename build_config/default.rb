MRuby::Build.new do |conf|

  conf.toolchain


  conf.cc.defines << "PICORB_VM_MRUBYC"

  conf.linker.library_paths << '/opt/homebrew/opt/openssl@3/lib'
  conf.cc.include_paths << '/opt/homebrew/opt/openssl@3/include'

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


MRuby::Build.new do |conf|

  conf.toolchain

  conf.picoruby

  conf.cc.defines << "MRC_INT64"
  conf.cc.defines << "MRBC_USE_HAL_POSIX"

  conf.cc.defines << "MRBC_ALLOC_LIBC"
  #conf.cc.defines << "MRBC_USE_ALLOC_PROF"
  #conf.cc.defines << 'MRC_CUSTOM_ALLOC'

  conf.cc.defines << "PICORB_WITH_IO_PREAD_PWRITE"
  conf.cc.defines << "MRBC_NO_OBJECT_STDOUT"

  conf.gembox "minimum"
  conf.gembox "posix"
  conf.gembox "r2p2"
  conf.gem core: "picoruby-bin-r2p2"
  conf.gem core: 'picoruby-net'

  conf.linker.flags_after_libraries << '-lcrypto'
  conf.linker.flags_after_libraries << '-lssl'
end



MRuby::Build.new do |conf|

  conf.toolchain

  conf.picoruby

  conf.cc.defines << "MRC_INT64"
  conf.cc.defines << "MRBC_USE_HAL_POSIX"
  conf.cc.defines << "MRBC_ALLOC_LIBC"

  conf.gembox "minimum"
  conf.gembox "r2p2"
  conf.gem core: "picoruby-bin-r2p2"
  conf.gem core: 'picoruby-net'

  conf.linker.flags_after_libraries << '-lcrypto'
  conf.linker.flags_after_libraries << '-lssl'
end


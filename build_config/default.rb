MRuby::Build.new do |conf|

  conf.toolchain

  conf.picoruby

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
  conf.gem core: "picoruby-net"
  conf.gem core: "picoruby-picotest"


  conf.linker.flags_after_libraries << '-lcrypto'
  conf.linker.flags_after_libraries << '-lssl'
end


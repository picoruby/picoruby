MRuby::Build.new do |conf|

  conf.toolchain

  conf.cc.defines << "PICORB_VM_MRUBYC"

#  conf.cc.defines << "MRBC_NO_STDIO"
  conf.cc.defines << "PICORUBY_INT64"
  conf.cc.defines << "MRBC_USE_HAL_POSIX"

  conf.cc.defines << "MRBC_TICK_UNIT=4"
  conf.cc.defines << "MRBC_TIMESLICE_TICK_COUNT=3"
  conf.cc.defines << "PICORB_PLATFORM_POSIX"

  conf.gembox "minimum"
  conf.gembox "posix"
  conf.gembox "stdlib"
  conf.gembox "utils"
  conf.gem core: "picoruby-net"
  conf.gem core: "picoruby-machine"
  conf.gem core: "picoruby-mbedtls"
  conf.gem core: 'picoruby-base64'

  conf.picoruby(alloc_libc: true)

end


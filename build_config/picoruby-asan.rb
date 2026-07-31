MRuby::Build.new do |conf|
  conf.toolchain :gcc

  conf.cc.defines << "MRB_TICK_UNIT=4"
  conf.cc.defines << "MRB_TIMESLICE_TICK_COUNT=3"

  conf.cc.defines << "PICORB_PLATFORM_POSIX"

  conf.cc.defines << "MRB_INT64"
  conf.cc.defines << "MRB_NO_BOXING"
  conf.cc.defines << "MRB_UTF8_STRING"

  conf.cc.flags << "-fsanitize=address"
  conf.cc.flags << "-fno-omit-frame-pointer"
  conf.cc.flags << "-g"
  conf.linker.flags << "-fsanitize=address"

  # Device-parity memory conditions: estalloc with a small pool and the
  # 0xaa free-fill, so the OOM -> full-GC path fires like on pico2_w.
  conf.cc.defines << "ESTALLOC_DEBUG"
  conf.cc.defines << "HEAP_SIZE=327680"

  conf.picoruby

  # Link OpenSSL libraries for socket SSL support
  conf.linker.libraries << 'ssl'
  conf.linker.libraries << 'crypto'

  conf.gembox "mruby-posix"
  conf.gembox "minimum"
  conf.gembox "core"
  conf.gembox "stdlib"
  conf.gembox "shell"
  conf.gembox "networking"
  conf.gem core: "picoruby-shinonome"
  conf.gem core: "picoruby-bin-r2p2"
  conf.gem core: "picoruby-bin-picoruby"
end

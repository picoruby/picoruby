MRuby::Build.new do |conf|
  conf.toolchain :gcc

  conf.cc.defines << "MRB_TICK_UNIT=4"
  conf.cc.defines << "MRB_TIMESLICE_TICK_COUNT=3"

  conf.cc.defines << "PICORB_ALLOC_ALIGN=8"
  conf.cc.defines << "PICORB_ALLOC_ESTALLOC"
  conf.cc.defines << "ESTALLOC_DEBUG"

  conf.cc.defines << "MRB_UTF8_STRING"

  conf.posix
  conf.microruby

  # Link OpenSSL libraries for socket SSL support
  conf.linker.libraries << 'ssl'
  conf.linker.libraries << 'crypto'

  conf.gembox "mruby-posix"
  conf.gembox "minimum"
  conf.gembox "core"
  conf.gembox "stdlib"
  conf.gem core: 'picoruby-bin-microruby'
  conf.gem core: 'picoruby-picotest'
end

MRuby::Build.new do |conf|
  conf.toolchain :gcc

  conf.cc.defines << "MRB_TICK_UNIT=4"
  conf.cc.defines << "MRB_TIMESLICE_TICK_COUNT=3"

  conf.cc.defines << "PICORB_ALLOC_ALIGN=8"
  conf.cc.defines << "PICORB_ALLOC_ESTALLOC"
  conf.cc.defines << "ESTALLOC_DEBUG"
  conf.cc.defines << "PICORB_PLATFORM_POSIX"

  conf.gem core: 'mruby-compiler2'
  conf.gem core: 'mruby-bin-mrbc2'
  conf.gem core: 'picoruby-bin-microruby'
  conf.gem core: 'picoruby-net'
  conf.gem core: 'picoruby-mbedtls'
  conf.gem core: 'picoruby-require'
  conf.gem core: 'picoruby-picotest'
  conf.gembox "stdlib-microruby"
  conf.gembox "posix-microruby"
  conf.gembox "r2p2"
  conf.gembox "utils"

  conf.microruby
end

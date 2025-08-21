MRuby::Build.new do |conf|

  conf.toolchain

  conf.cc.defines << "PICORB_VM_MRUBYC"

  conf.cc.defines << "PICORUBY_INT64"
  conf.cc.defines << "MRBC_USE_HAL_POSIX"

  conf.cc.defines << "MRBC_TICK_UNIT=4"
  conf.cc.defines << "MRBC_TIMESLICE_TICK_COUNT=3"
  conf.cc.defines << "PICORB_PLATFORM_POSIX"

  conf.gem core: 'mruby-compiler2'
  conf.gem core: 'mruby-bin-mrbc2'
  conf.gem core: 'picoruby-bin-microruby'
  conf.gem core: 'picoruby-mrubyc'
  conf.gem core: 'picoruby-require'
  conf.gem core: 'picoruby-posix-io'
  conf.gem core: 'picoruby-dir'
  conf.gem core: "picoruby-picotest"
  conf.gem core: "picoruby-picorubyvm"
  conf.gem core: "picoruby-metaprog"
  conf.gem core: "picoruby-pack"
  conf.gem core: "picoruby-time-class"
  conf.gem core: "picoruby-machine"

  conf.picoruby(alloc_libc: true)

end

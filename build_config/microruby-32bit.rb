MRuby::CrossBuild.new('microruby-32bit') do |conf|
  conf.toolchain :gcc

  conf.cc.defines << "PICORB_PLATFORM_POSIX"
  conf.cc.defines << "MRB_TICK_UNIT=4"
  conf.cc.defines << "MRB_TIMESLICE_TICK_COUNT=3"

  conf.cc.defines << "PICORB_ALLOC_ALIGN=4"
  conf.cc.defines << "PICORB_ALLOC_ESTALLOC"
  conf.cc.defines << "ESTALLOC_DEBUG"

  conf.cc.defines << "MRB_INT64"
  conf.cc.defines << "MRB_32BIT"

  conf.cc.flags << '-m32'
  conf.cc.flags << '-static'
  conf.cc.flags << '-Wall'
  conf.cc.flags << '-Wextra'
  conf.cc.flags << '-Werror=address-of-packed-member'
  conf.cc.flags << '-Wno-unused-parameter'
  conf.cc.flags << '-mno-stackrealign'
  conf.cc.flags << '-falign-functions=2'
  conf.cc.flags << '-falign-jumps=2'
  conf.cc.flags << '-falign-loops=2'
  conf.cc.flags << '-falign-labels=2'
  conf.linker.flags << '-m32'

  conf.gem core: 'mruby-compiler2'
  conf.gem core: 'mruby-bin-mrbc2'
  conf.gem core: 'picoruby-bin-microruby'
  #conf.gem core: 'picoruby-net'
  #conf.gem core: 'picoruby-mbedtls'
  conf.gem core: 'picoruby-require'
  conf.gem core: 'picoruby-picotest'
  conf.gembox "stdlib-microruby"
  conf.gembox "posix-microruby"
  conf.gembox "r2p2"
  conf.gembox "utils"

  conf.microruby
end

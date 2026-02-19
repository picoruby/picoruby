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

  conf.cc.defines << "MRB_UTF8_STRING"

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

  conf.microruby

  conf.gembox "minimum"
  conf.gembox "core"
  conf.gembox "stdlib"
  conf.gembox "shell"
  conf.gem core: "picoruby-shinonome"
  conf.gem core: "picoruby-bin-r2p2"
end

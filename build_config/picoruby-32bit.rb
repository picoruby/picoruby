MRuby::CrossBuild.new('picoruby-32bit') do |conf|
  conf.toolchain :gcc
  # A CrossBuild selects no gem port by itself
  conf.ports :posix

  conf.cc.defines << "PICORB_PLATFORM_POSIX"
  conf.cc.defines << "MRB_TICK_UNIT=4"
  conf.cc.defines << "MRB_TIMESLICE_TICK_COUNT=3"

  conf.cc.defines << "ESTALLOC_DEBUG"

  conf.cc.defines << "MRB_INT64"
  conf.cc.defines << "MRB_NO_BOXING"
  conf.cc.defines << "MRB_32BIT"

  conf.cc.defines << "MRB_UTF8_STRING"

  # glibc on i386 keeps a 32-bit time_t unless asked; Time past 2038
  # would break down into the wrong calendar fields.
  conf.cc.defines << "_TIME_BITS=64"
  conf.cc.defines << "_FILE_OFFSET_BITS=64"

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

  conf.picoruby(alloc_align: 4)

  conf.gembox "minimum"
  conf.gembox "core"
  conf.gembox "stdlib"
  conf.gembox "shell"
  conf.gem core: "picoruby-shinonome"
  conf.gem core: "picoruby-bin-r2p2"
end

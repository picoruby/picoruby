MRuby::CrossBuild.new("microruby-cortex-m33") do |conf|

  ###############################################################
  # You need following tools:
  #   arm-none-eabi       | to make libmruby.a
  ###############################################################

  conf.toolchain("gcc")

  conf.cc.command = "arm-none-eabi-gcc"
  conf.linker.command = "arm-none-eabi-ld"
  conf.linker.flags << "-static"
  conf.archiver.command = "arm-none-eabi-ar"

  conf.cc.host_command = "gcc"

  conf.cc.flags.flatten!
  conf.cc.flags << "-mcpu=cortex-m33"
  conf.cc.flags << "-mthumb"

  conf.cc.flags << "-fno-strict-aliasing"
  conf.cc.flags << "-fno-unroll-loops"
  conf.cc.flags << "-mslow-flash-data"

  conf.cc.flags << "-fshort-enums"

  conf.cc.flags << "-Wall"
  conf.cc.flags << "-Wno-format"
  conf.cc.flags << "-Wno-unused-function"
  conf.cc.flags << "-Wno-maybe-uninitialized"
  conf.cc.flags << "-ffunction-sections"
  conf.cc.flags << "-fdata-sections"

  # These defines should not contradict platform's configuration
  conf.cc.defines << "PICORUBY_INT64"

  conf.gem github: 'picoruby/mruby-compiler2'
  conf.gem core: "picoruby-mruby"

  conf.cc.include_paths << conf.gems['picoruby-mruby'].dir + '/lib/mruby/include'
  conf.cc.defines << "PICORB_VM_MRUBY"
end

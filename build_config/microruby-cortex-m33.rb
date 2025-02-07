MRuby::CrossBuild.new("microruby-cortex-m33") do |conf|

  ###############################################################
  # You need following tools:
  #   arm-none-eabi       | to make libmruby.a
  ###############################################################

  conf.toolchain("gcc")

  conf.cc.host_command = "gcc"

  conf.cc.command = "arm-none-eabi-gcc"
  conf.linker.command = "arm-none-eabi-ld"
  conf.linker.flags << "-static"
  conf.archiver.command = "arm-none-eabi-ar"

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

  conf.gem github: 'picoruby/mruby-compiler2'
  conf.gem core: "picoruby-mruby"

  conf.microruby
end

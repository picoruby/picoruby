MRuby::CrossBuild.new("prsh-cortex-m0plus") do |conf|

  ###############################################################
  # You need following tools:
  #   arm-none-eabi       | to make libmruby.a
  #   arm-linux-gnueabihf | to make lemon and ptr_size_generator
  #   qemu-arm-static     | to run ptr_size_generator
  ###############################################################

  conf.toolchain

  conf.cc.command = "arm-none-eabi-gcc"
  conf.linker.command = "arm-none-eabi-ld"
  conf.linker.flags << "-static"
  conf.archiver.command = "arm-none-eabi-ar"

  conf.cc.alt_command = "arm-linux-gnueabihf-gcc"
  ENV['QEMU'] = "qemu-arm-static"

  conf.cc.flags.flatten!
  conf.cc.flags.delete "-O3"
  conf.cc.flags << "-Og"
  conf.cc.flags << "-mcpu=cortex-m0plus"
  conf.cc.flags << "-mthumb"
  conf.cc.flags << "-Wall"
  conf.cc.flags << "-Wno-format"
  conf.cc.flags << "-Wno-unused-function"
  conf.cc.flags << "-Wno-maybe-uninitialized"
  conf.cc.flags << "-ffunction-sections"
  conf.cc.flags << "-fdata-sections"

  # These defines should correspond to
  # the platform's configuration
  conf.cc.defines << "NDEBUG"
  conf.cc.defines << "MRBC_REQUIRE_32BIT_ALIGNMENT"
  conf.cc.defines << "MRBC_CONVERT_CRLF"
  conf.cc.defines << "MRBC_USE_MATH"
  conf.cc.defines << "MRBC_TICK_UNIT=1"
  conf.cc.defines << "MRBC_TIMESLICE_TICK_COUNT=10"
  conf.cc.defines << "LEMON_PICORBC"

  conf.mrubyc_hal_arm
  conf.picoruby

  conf.gem core: "picoruby-filesystem-fat"
  conf.gem core: "picoruby-vim"
  conf.gem core: "picoruby-shell"
  conf.gem core: "picoruby-terminal"
  conf.gem core: "picoruby-sandbox"

end



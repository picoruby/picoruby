MRuby::CrossBuild.new("prk_firmware-cortex-m0plus") do |conf|

  ###############################################################
  # You need following tools:
  #   arm-none-eabi       | to make libmruby.a
  ###############################################################

  conf.toolchain

  conf.cc.command = "arm-none-eabi-gcc"
  conf.linker.command = "arm-none-eabi-ld"
  conf.linker.flags << "-static"
  conf.archiver.command = "arm-none-eabi-ar"

  conf.cc.host_command = "gcc"

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
  conf.cc.defines << "LEMON_PICORBC"
  conf.cc.defines << "USE_FAT_FLASH_DISK"
  conf.cc.defines << "NO_CLOCK_GETTIME"

  conf.mrubyc_hal_arm
  conf.picoruby

  conf.gembox "prk_firmware"
  if ENV['PICORUBY_SQLITE3']
    conf.gem :core => "picoruby-sqlite3"
  end
end



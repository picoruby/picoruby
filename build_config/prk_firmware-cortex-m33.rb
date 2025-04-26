MRuby::CrossBuild.new("prk_firmware-cortex-m33") do |conf|

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
  conf.cc.flags << "-mcpu=cortex-m33"
  conf.cc.flags << "-mthumb"

  conf.cc.flags << "-fno-strict-aliasing"
  conf.cc.flags << "-fno-unroll-loops"
  conf.cc.flags << "-mslow-flash-data"

  conf.cc.flags << "-fshort-enums"

  conf.cc.flags << "-Wall"
  conf.cc.flags << "-Wno-format"
  conf.cc.flags << "-Wno-unused-function"
  conf.cc.flags << "-ffunction-sections"
  conf.cc.flags << "-fdata-sections"

  # These defines should correspond to
  # the platform's configuration
  conf.cc.defines << "PICORUBY_INT64"
  conf.cc.defines << "MRBC_REQUIRE_32BIT_ALIGNMENT=1"
  conf.cc.defines << "MRBC_CONVERT_CRLF=1"
  conf.cc.defines << "MRBC_USE_FLOAT=2"
  conf.cc.defines << "MRBC_USE_MATH=1"
  conf.cc.defines << "MRBC_TICK_UNIT=1"
  conf.cc.defines << "MRBC_TIMESLICE_TICK_COUNT=10"
  conf.cc.defines << "USE_FAT_FLASH_DISK=1"
  conf.cc.defines << "NO_CLOCK_GETTIME=1"
  conf.cc.defines << "MAX_SYMBOLS_COUNT=1800"
  if ENV['PICORUBY_SQLITE3']
    conf.gem core: 'picoruby-sqlite3'
    conf.gem core: 'picoruby-adafruit_pcf8523'
  end
  if ENV['PICORUBY_SD_CARD']
    conf.cc.defines << "USE_FAT_SD_DISK=1"
  end

  conf.gembox "prk_firmware"

  conf.mrubyc_hal_arm
  conf.picoruby(alloc_libc: false)

end




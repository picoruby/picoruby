MRuby::CrossBuild.new("r2p2_w-microruby-cortex-m0plus") do |conf|

  ###############################################################
  # You need following tools:
  #   arm-none-eabi       | to make libmruby.a
  ###############################################################

  conf.toolchain("gcc")

  # MicroRuby specific defines (MRB_ prefix, not MRBC_)
  conf.cc.defines << "MRB_TICK_UNIT=1"
  conf.cc.defines << "MRB_TIMESLICE_TICK_COUNT=10"

  conf.cc.defines << "MRB_INT64"
  conf.cc.defines << "MRB_32BIT"
  conf.cc.defines << "PICORB_ALLOC_ESTALLOC"
  conf.cc.defines << "ESTALLOC_DEBUG"
  conf.cc.defines << "USE_FAT_FLASH_DISK=1"
  conf.cc.defines << "USE_WIFI"

  # Cortex-M0+ toolchain settings
  conf.cc.command = "arm-none-eabi-gcc"
  conf.linker.command = "arm-none-eabi-ld"
  conf.linker.flags << "-static"
  conf.archiver.command = "arm-none-eabi-ar"

  conf.cc.host_command = "gcc"

  conf.cc.flags.flatten!
  conf.cc.flags << "-mcpu=cortex-m0plus"
  conf.cc.flags << "-mthumb"
  conf.cc.flags << "-fshort-enums"
  conf.cc.flags << "-Wall"
  conf.cc.flags << "-Wno-format"
  conf.cc.flags << "-Wno-unused-function"
  conf.cc.flags << "-ffunction-sections"
  conf.cc.flags << "-fdata-sections"

  # gemboxesとgemsの設定
  conf.gembox "stdlib-microruby"
  conf.gembox "baremetal"
  conf.gembox "peripherals"
  conf.gembox "r2p2"
  conf.gembox "cyw43"
  conf.gembox "peripheral_utils"
  conf.gembox "utils"

  # ワイヤレス接続関連のgem
  conf.gem core: 'picoruby-jwt'
  conf.gem core: 'picoruby-net'
  conf.gem core: 'picoruby-mbedtls'
  conf.gem core: 'picoruby-cyw43'
  conf.gem core: 'picoruby-ble'

  # MicroRuby設定の有効化
  conf.microruby
end

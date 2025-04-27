MRuby::CrossBuild.new("r2p2_w-microruby-cortex-m33") do |conf|

  ###############################################################
  # You need following tools:
  #   arm-none-eabi       | to make libmruby.a
  ###############################################################

  conf.toolchain("gcc")

  conf.cc.defines << "MRB_TICK_UNIT=1"
  conf.cc.defines << "MRB_TIMESLICE_TICK_COUNT=10"

  conf.cc.defines << "MRB_INT64"
  conf.cc.defines << "MRB_32BIT"
  conf.cc.defines << "PICORB_ALLOC_ESTALLOC"
  conf.cc.defines << "ESTALLOC_DEBUG"
  conf.cc.defines << "USE_FAT_FLASH_DISK=1"
  conf.cc.defines << "USE_WIFI"

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

  conf.gem core: 'mruby-compiler2'
  conf.gem core: "picoruby-mruby"
  conf.gem core: "picoruby-json"
  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-string-ext"
  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-array-ext"
  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-time"
  conf.gem core: "picoruby-env"
  conf.gem core: "picoruby-io-console"
  conf.gem core: "picoruby-picorubyvm"
  conf.gem core: "picoruby-sandbox"
  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-objectspace"
  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-metaprog"
  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-error"
  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-sprintf"
  conf.gem core: "picoruby-shell"
  conf.gem core: "picoruby-vfs"
  conf.gem core: "picoruby-filesystem-fat"
  conf.gem core: "picoruby-watchdog"
  conf.gem core: "picoruby-rng"
  conf.gem core: "picoruby-gpio"
  conf.gem core: "picoruby-adc"
  conf.gem core: "picoruby-i2c"
  conf.gem core: "picoruby-pwm"
  conf.gem core: "picoruby-spi"
  conf.gem core: "picoruby-uart"
  conf.gem core: "picoruby-mbedtls"
  conf.gem core: "picoruby-net"
  conf.gem core: "picoruby-base16"
  conf.gem core: "picoruby-base64"
  conf.gem core: "picoruby-ble"
  conf.gem core: "picoruby-yaml"
  conf.gem core: "picoruby-vim"
  conf.gem core: "picoruby-rng"

#  # stdlib-microruby
##  conf.gembox "stdlib-microruby"
#  conf.gem core: 'mruby-compiler2'
#  conf.gem core: 'picoruby-machine'
#  conf.gem core: "picoruby-mruby"
#  conf.gem core: "picoruby-base16"
#  conf.gem core: "picoruby-base64"
#  conf.gem core: "picoruby-json"
#  conf.gem core: "picoruby-yaml"
#  conf.gem core: "picoruby-picorubyvm"
#  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-string-ext"
#  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-array-ext"
#  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-time"
#  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-objectspace"
#  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-metaprog"
#  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-error"
#  conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-sprintf"
#
#  # baremetal
##  conf.gembox "baremetal"
#  conf.gem core: "picoruby-vfs"
#  conf.gem core: "picoruby-filesystem-fat"
#  conf.gem core: "picoruby-watchdog"
#
#  # peripherals
##  conf.gembox "peripherals"
#  conf.gem core: "picoruby-gpio"
#  conf.gem core: "picoruby-i2c"
#  conf.gem core: "picoruby-spi"
#  conf.gem core: "picoruby-adc"
#  conf.gem core: "picoruby-uart"
#  conf.gem core: "picoruby-pwm"
#
#  # r2p2
##  conf.gembox "r2p2"
#  conf.gem core: "picoruby-shell"
#
#  # cyw43
##  conf.gembox "cyw43"
#  conf.gem core: "picoruby-mbedtls"
#  conf.gem core: "picoruby-net"
#  conf.gem core: "picoruby-ble"
#
#  conf.gem core: "picoruby-env"
#  conf.gem core: "picoruby-io-console"
#  conf.gem core: "picoruby-sandbox"
#  conf.gem core: "picoruby-rng"

##  conf.gembox "peripheral_utils"
##  conf.gembox "utils"
  conf.microruby
end

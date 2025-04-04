MRuby::CrossBuild.new("r2p2-microruby-cortex-m33") do |conf|

  ###############################################################
  # You need following tools:
  #   arm-none-eabi       | to make libmruby.a
  ###############################################################

  conf.toolchain("gcc")

  conf.cc.defines << "MRB_TICK_UNIT=1"
  conf.cc.defines << "MRB_TIMESLICE_TICK_COUNT=10"

  conf.cc.defines << "USE_FAT_FLASH_DISK=1"
  conf.cc.defines << "PICORB_ALLOC_ESTALLOC"
#  conf.cc.defines << "MRB_NAN_BOXING"

  conf.cc.command = "arm-none-eabi-gcc"
  conf.linker.command = "arm-none-eabi-ld"
  conf.linker.flags << "-static"
  conf.archiver.command = "arm-none-eabi-ar"

  conf.cc.host_command = "gcc"

  conf.cc.flags.flatten!
  conf.cc.flags << "-mcpu=cortex-m33"
#  conf.cc.flags << "-march=armv8-m.main+fp+dsp"
#  conf.cc.flags << "-mabi=aapcs-linux"
#  conf.cc.flags << "-mfloat-abi=softfp"
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
  #conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-io"
  #conf.gem core: "mruby-file-stat"
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
  conf.gem core: "picoruby-shell"
  #conf.gem gemdir: "mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-dir"

  conf.gem core: "picoruby-vfs"
  conf.gem core: "picoruby-filesystem-fat"

  conf.gem core: "picoruby-watchdog"
  conf.gem core: "picoruby-rng"

  conf.microruby
end



MRuby::CrossBuild.new("esp32") do |conf|
  conf.toolchain("gcc")

  conf.cc.command = "riscv32-esp-elf-gcc"
  conf.linker.command = "riscv32-esp-elf-ld"
  conf.archiver.command = "riscv32-esp-elf-ar"

  conf.cc.host_command = "gcc"
  conf.cc.flags << "-Wall"
  conf.cc.flags << "-Wno-format"
  conf.cc.flags << "-Wno-unused-function"
  conf.cc.flags << "-Wno-maybe-uninitialized"

  conf.cc.defines << "MRBC_TICK_UNIT=10"
  conf.cc.defines << "MRBC_TIMESLICE_TICK_COUNT=1"
  conf.cc.defines << "MRBC_USE_FLOAT=2"
  conf.cc.defines << "MRBC_CONVERT_CRLF=1"
  conf.cc.defines << "USE_FAT_FLASH_DISK"
  conf.cc.defines << "NDEBUG"

  conf.gembox 'peripherals'
  conf.gembox 'r2p2'
  conf.gem core: "picoruby-machine"
  conf.gem core: "picoruby-picorubyvm"
  conf.gem core: "picoruby-rng"
  conf.gem core: "picoruby-watchdog"
  conf.gem core: "picoruby-rmt"
  conf.gem core: "picoruby-adafruit_sk6812"
  conf.gem core: "picoruby-yaml"
  conf.gem core: "picoruby-vim"
  conf.gem core: "picoruby-picoline"
  conf.gem core: "picoruby-base64"
  conf.gem core: "picoruby-mbedtls"
  conf.picoruby(alloc_libc: false)
end

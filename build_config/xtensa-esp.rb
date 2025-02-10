MRuby::CrossBuild.new("esp32") do |conf|
  conf.toolchain("gcc")

  conf.cc.command = "xtensa-esp32-elf-gcc"
  conf.linker.command = "xtensa-esp32-elf-ld"
  conf.archiver.command = "xtensa-esp32-elf-ar"

  conf.cc.host_command = "gcc"
  conf.cc.flags << "-Wall"
  conf.cc.flags << "-Wno-format"
  conf.cc.flags << "-Wno-unused-function"
  conf.cc.flags << "-Wno-maybe-uninitialized"
  conf.cc.flags << "-mlongcalls"

  conf.cc.defines << "MRBC_TICK_UNIT=10"
  conf.cc.defines << "MRBC_TIMESLICE_TICK_COUNT=1"
  conf.cc.defines << "MRBC_USE_FLOAT=2"
  conf.cc.defines << "MRC_CUSTOM_ALLOC"
  conf.cc.defines << "MRBC_CONVERT_CRLF=1"
  conf.cc.defines << "USE_FAT_FLASH_DISK"
  conf.cc.defines << "NDEBUG"

  conf.gem core: "picoruby-machine"
  conf.gem core: "picoruby-shell"
  conf.gem core: "picoruby-picorubyvm"
  conf.picoruby(alloc_libc: false)
end

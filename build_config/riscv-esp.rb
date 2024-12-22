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
  conf.cc.defines << "NDEBUG"

  conf.gem core: "picoruby-machine"
  conf.gem core: "picoruby-shell"
  conf.gem core: "picoruby-picorubyvm"
  conf.picoruby
end

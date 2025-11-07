MRuby::CrossBuild.new('esp32-microruby') do |conf|
  conf.toolchain('gcc')

  conf.cc.command = 'riscv32-esp-elf-gcc'
  conf.linker.command = 'riscv32-esp-elf-ld'
  conf.archiver.command = 'riscv32-esp-elf-ar'

  conf.cc.host_command = 'gcc'
  conf.cc.flags << '-Wall'
  conf.cc.flags << '-Wno-format'
  conf.cc.flags << '-Wno-unused-function'
  conf.cc.flags << '-Wno-maybe-uninitialized'

  conf.cc.defines << 'MRBC_TICK_UNIT=10'
  conf.cc.defines << 'MRBC_TIMESLICE_TICK_COUNT=1'
  conf.cc.defines << 'MRBC_USE_FLOAT=2'
  conf.cc.defines << 'MRBC_CONVERT_CRLF=1'
  conf.cc.defines << 'USE_FAT_FLASH_DISK'
  conf.cc.defines << 'NDEBUG'
  conf.cc.defines << 'PICORB_ALLOC_ESTALLOC'
  conf.cc.defines << 'ESTALLOC_DEBUG'

  conf.gem core: 'mruby-compiler2'
  conf.gem core: "picoruby-mruby"
  conf.gem core: 'picoruby-machine'
  conf.microruby
end

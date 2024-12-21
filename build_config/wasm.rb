MRuby::CrossBuild.new("wasm") do |conf|
  toolchain :clang

  conf.cc.defines << "PICORUBY_PLATFORM=posix"
  conf.cc.defines << "PICORUBY_INT64=1"
  conf.cc.defines << "MRBC_ALLOC_LIBC=1"
  conf.cc.defines << "MRBC_NO_TIMER=1"
  conf.cc.defines << "MRBC_USE_HAL_POSIX=1"
  conf.cc.defines << "MRBC_TICK_UNIT=10"
  conf.cc.defines << "MRBC_TIMESLICE_TICK_COUNT=1"

  conf.cc.command = 'emcc'
  conf.linker.command = 'emcc'
  conf.archiver.command = 'emar'

  conf.gem core: 'picoruby-mrubyc'
  conf.gem core: 'picoruby-require'
  conf.gem core: 'picoruby-wasm'

  conf.picoruby
end

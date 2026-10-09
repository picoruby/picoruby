MRuby::CrossBuild.new("picoruby-wasm-test") do |conf|
  toolchain :clang
  # A CrossBuild selects no gem port by itself; Emscripten's libc is POSIX
  conf.ports :posix

  conf.cc.defines << "PICORB_PLATFORM_POSIX"
  conf.cc.defines << "PICORB_PLATFORM_WASM"
  conf.cc.defines << "MRB_TICK_UNIT=4"
  conf.cc.defines << "MRB_TIMESLICE_TICK_COUNT=1"

  # 32-bit word boxing: mrb_value is 4 bytes. An Integer past 31 bits is a
  # heap RInteger, and one past mrb_int is a Bignum from mruby-bigint.
  conf.cc.defines << "MRB_32BIT"
  conf.cc.defines << "MRB_INT32"
  conf.cc.defines << "MRB_WORD_BOXING"
  conf.cc.defines << "MRB_UTF8_STRING"
  conf.cc.defines << "PICORB_DEBUG"

  conf.cc.command = 'emcc'
  conf.linker.command = 'emcc'
  conf.archiver.command = 'emar'

  conf.picoruby(alloc_estalloc: false)

  conf.gem gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-bigint"
  conf.gembox "mruby-posix"
  conf.gembox "stdlib"

  conf.gem core: 'picoruby-wasm'
  conf.gem core: 'picoruby-picotest'
  conf.gem core: 'picoruby-funicular'
end

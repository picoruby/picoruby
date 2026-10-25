MRuby::CrossBuild.new("picoruby-wasm#{ENV['PICORB_DEBUG'] ? '-debug' : ''}") do |conf|
  conf.generate_package_json_from_template(
    "#{MRUBY_ROOT}/mrbgems/picoruby-wasm/npm/picoruby/package.json.template",
    "#{MRUBY_ROOT}/mrbgems/picoruby-wasm/npm/picoruby/package.json"
  )
  conf.generate_package_json_from_template(
    "#{MRUBY_ROOT}/mrbgems/picoruby-wasm/npm/mrbc/package.json.template",
    "#{MRUBY_ROOT}/mrbgems/picoruby-wasm/npm/mrbc/package.json"
  )

  conf.toolchain :clang
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
  # The method cache: mruby's default of 256 entries thrashes under a
  # browser app's class count (a VDOM diff spent a tenth of its time in
  # mrb_vm_find_method). Build-wide: the cache sits in mrb_state.
  conf.cc.defines << "MRB_METHOD_CACHE_SIZE=1024"

  conf.cc.command = 'emcc'
  conf.linker.command = 'emcc'
  conf.archiver.command = 'emar'
  # setjmp/longjmp (mruby's exceptions, mrb_protect, the funcall
  # boundary) through WebAssembly exceptions, not through JS: with the
  # default (-sSUPPORT_LONGJMP=emscripten) every call out of a function
  # that holds a setjmp crosses into JS and back (invoke_*). Every object
  # of the build needs the flag; the link (mrbgem.rake) adds the
  # matching -sSUPPORT_LONGJMP=wasm.
  conf.cc.flags << "-fwasm-exceptions"

  conf.picoruby(alloc_estalloc: false)
  # picoruby-aot: the gems that declare spec.aot (funicular's VDOM path
  # among them) compile their typed defs to C
  conf.aot
  conf.gembox "mruby-posix"
  conf.gembox "stdlib"

  if ENV['PICORB_DEBUG']
    conf.cc.defines << "MRB_USE_DEBUG_HOOK"
    conf.gem gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-binding"
  end
  conf.gem gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-math"
  conf.gem gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-bigint"
  conf.gem core: 'picoruby-wasm'
  conf.gem core: 'picoruby-indexeddb'
  conf.gem core: 'picoruby-funicular'
  conf.gem core: 'picoruby-markdown'
  conf.gem core: 'picoruby-drb'
  conf.gem core: 'picoruby-sqlite3'
end

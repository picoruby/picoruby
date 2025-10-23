MRuby::CrossBuild.new("wasm") do |conf|
  toolchain :clang

  conf.cc.defines << "PICORUBY_INT64=1"
  conf.cc.defines << "MRBC_USE_FLOAT=2"
  conf.cc.defines << "MRBC_ALLOC_LIBC=1"
  conf.cc.defines << "MRBC_NO_TIMER=1"
  conf.cc.defines << "MRBC_TICK_UNIT=17"
  conf.cc.defines << "MRBC_TIMESLICE_TICK_COUNT=1"

  conf.cc.command = 'emcc'
  conf.linker.command = 'emcc'
  conf.archiver.command = 'emar'

  conf.posix
  conf.picoruby(alloc_libc: true)

  conf.gem core: 'picoruby-mrubyc'
  conf.gem core: 'picoruby-require'
  conf.gem core: 'picoruby-wasm'
  conf.gem core: 'picoruby-base16'
  conf.gem core: 'picoruby-base64'
  conf.gem core: 'picoruby-data'
  conf.gem core: 'picoruby-jwt'
  conf.gem core: 'picoruby-pack'
  conf.gem core: 'picoruby-picorubyvm'
  conf.gem core: 'picoruby-rng'
  conf.gem core: 'picoruby-yaml'
end

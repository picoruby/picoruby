MRuby::CrossBuild.new("picoruby-wasm-test") do |conf|
  toolchain :clang

  conf.cc.defines << "PICORB_PLATFORM_POSIX"
  conf.cc.defines << "PICORB_PLATFORM_WASM"
  conf.cc.defines << "MRB_TICK_UNIT=4"
  conf.cc.defines << "MRB_TIMESLICE_TICK_COUNT=1"

  conf.cc.defines << "MRB_32BIT"
  conf.cc.defines << "MRB_INT64"
  conf.cc.defines << "MRB_NO_BOXING"
  conf.cc.defines << "MRB_UTF8_STRING"
  conf.cc.defines << "PICORB_DEBUG"

  conf.cc.command = 'emcc'
  conf.linker.command = 'emcc'
  conf.archiver.command = 'emar'

  conf.picoruby

  conf.gembox "mruby-posix"
  conf.gembox "stdlib"

  conf.gem core: 'picoruby-wasm'
  conf.gem core: 'picoruby-picotest'
end

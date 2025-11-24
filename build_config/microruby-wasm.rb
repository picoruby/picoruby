MRuby::CrossBuild.new("microruby-wasm") do |conf|
  toolchain :clang

  conf.cc.defines << "MRB_TICK_UNIT=4"
  conf.cc.defines << "MRB_TIMESLICE_TICK_COUNT=3"

  conf.cc.defines << "MRB_INT64"

  conf.cc.command = 'emcc'
  conf.linker.command = 'emcc'
  conf.archiver.command = 'emar'

  conf.posix
  conf.microruby

  # Add mruby-io for POSIX I/O support
  conf.gem gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-io"

  conf.gem core: 'picoruby-wasm'
  conf.gem core: 'picoruby-machine'
  conf.gem core: 'picoruby-base16'
  conf.gem core: 'picoruby-base64'
  conf.gem core: 'picoruby-data'
  conf.gem core: 'picoruby-jwt'
  conf.gem core: 'picoruby-pack'
  conf.gem core: 'picoruby-rng'
  conf.gem core: 'picoruby-yaml'
end

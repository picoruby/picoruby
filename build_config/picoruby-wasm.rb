MRuby::CrossBuild.new("picoruby-wasm") do |conf|
  toolchain :clang

  conf.cc.defines << "MRB_TICK_UNIT=17"
  conf.cc.defines << "MRB_TIMESLICE_TICK_COUNT=1"

  conf.cc.defines << "MRB_INT64"

  #conf.cc.defines << "MRB_USE_CXX_EXCEPTION=1"
  #conf.cc.flags << '-Wno-deprecated'
  #conf.cc.flags << '-Wno-reorder-init-list'
  #conf.linker.flags << '-Wno-reorder-init-list'
  #conf.cc.command = 'em++'
  #conf.linker.command = 'em++'

  conf.cc.command = 'emcc'
  conf.linker.command = 'emcc'
  conf.archiver.command = 'emar'

  conf.posix
  conf.microruby

  # Add mruby-io for POSIX I/O support
  conf.gem gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-io"
  conf.gem gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-math"
  conf.gem core: 'picoruby-wasm'
  conf.gem core: 'picoruby-json'
  conf.gem core: 'picoruby-funicular'
  conf.gem core: 'picoruby-base16'
  conf.gem core: 'picoruby-base64'
  conf.gem core: 'picoruby-data'
  if conf.vm_mruby?
    conf.gem gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-pack"
  elsif conf.vm_mrubyc?
    conf.gem core: 'picoruby-pack'
  end
  conf.gem core: 'picoruby-rng'
  conf.gem core: 'picoruby-yaml'
end

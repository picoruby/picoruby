MRuby::CrossBuild.new("picoruby-wasm") do |conf|
  # Generate package.json from template with version from version.h
  conf.generate_package_json_from_template(
    "#{MRUBY_ROOT}/mrbgems/picoruby-wasm/npm/package.json.template",
    "#{MRUBY_ROOT}/mrbgems/picoruby-wasm/npm/package.json"
  )

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

  conf.gembox "mruby-posix"
  conf.gembox "stdlib"

  if ENV['PICORUBY_DEBUG']
    conf.cc.defines << "MRB_USE_DEBUG_HOOK"
    conf.gem gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-binding"
  end
  # Add mruby-io for POSIX I/O support
  conf.gem gemdir: "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/mrbgems/mruby-io"
  conf.gem core: 'picoruby-wasm'
  conf.gem core: 'picoruby-funicular'
  conf.gem core: 'picoruby-markdown'
  conf.gem core: 'picoruby-drb-websocket'
end

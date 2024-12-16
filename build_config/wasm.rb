MRuby::CrossBuild.new("wasm") do |conf|
  toolchain :clang

  conf.cc.defines << "PICORUBY_PLATFORM=posix"

  conf.cc.command = 'emcc'
  conf.linker.command = 'emcc'
  conf.archiver.command = 'emar'

  conf.gem core: 'picoruby-mrubyc'
  conf.gem core: 'picoruby-require'
  conf.gem core: 'picoruby-wasm'

  conf.picoruby
end

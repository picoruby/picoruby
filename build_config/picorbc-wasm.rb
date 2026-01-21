MRuby::CrossBuild.new("picorbc-wasm") do |conf|
  toolchain :clang

  conf.cc.command = 'emcc'
  conf.linker.command = 'emcc'
  conf.archiver.command = 'emar'

  # Emscripten flags for Node.js execution
  conf.linker.flags << '-sWASM=1'
  conf.linker.flags << '-sNODERAWFS=1'
  conf.linker.flags << '-sALLOW_MEMORY_GROWTH=1'
  conf.linker.flags << '-sEXPORTED_RUNTIME_METHODS=["callMain"]'

  # Set executable extension to .js (generates both .js and .wasm)
  conf.exts.executable = '.js'

  conf.posix
  conf.microruby

  # Compiler gems
  conf.gem core: "mruby-compiler2"
  conf.gem core: "mruby-bin-mrbc2"

  # Set output binary name
  conf.instance_variable_set :@mrbcfile, "bin/picorbc.wasm"
  conf.disable_libmruby
  conf.disable_presym
end

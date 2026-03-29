MRuby::CrossBuild.new("picorbc-wasm") do |conf|
  # Generate package.json from template with version from version.h
  conf.generate_package_json_from_template(
    "#{MRUBY_ROOT}/mrbgems/picoruby-wasm/npm/picorbc/package.json.template",
    "#{MRUBY_ROOT}/mrbgems/picoruby-wasm/npm/picorbc/package.json"
  )

  toolchain :clang

  conf.set_build_info

  conf.cc.defines << 'PICORB_PLATFORM_WASM'
  conf.cc.defines << "PICORB_PLATFORM_POSIX"

  conf.cc.command = 'emcc'
  conf.linker.command = 'emcc'
  conf.archiver.command = 'emar'

  # Emscripten flags for Node.js execution
  conf.linker.flags << '-sWASM=1'
  conf.linker.flags << '-sNODERAWFS=1'
  conf.linker.flags << '-sALLOW_MEMORY_GROWTH=1'
  conf.linker.flags << '-sEXPORTED_RUNTIME_METHODS=["callMain"]'
  conf.linker.flags << '-sEXIT_RUNTIME=1'
  conf.linker.flags << '-sEXECUTABLE=1'

  # Set executable extension to .js (generates both .js and .wasm)
  conf.exts.executable = '.js'

  # Compiler gems
  conf.gem core: "mruby-compiler2"
  conf.gem core: "mruby-bin-mrbc2"

  # Set output binary name
  conf.instance_variable_set :@mrbcfile, "bin/picorbc.wasm"
  conf.disable_libmruby
end

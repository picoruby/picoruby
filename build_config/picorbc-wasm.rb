MRuby::CrossBuild.new("picorbc-wasm") do |conf|
  # Generate package.json from template with version from version.h
  package_json_template = "#{MRUBY_ROOT}/mrbgems/mruby-bin-mrbc2/npm/package.json.template"
  package_json = "#{MRUBY_ROOT}/mrbgems/mruby-bin-mrbc2/npm/package.json"
  if File.exist?(package_json_template)
    version = File.read("#{MRUBY_ROOT}/include/version.h")
                  .match(/#define PICORUBY_VERSION "(.+?)"/)[1]
    template_content = File.read(package_json_template)
    generated_content = template_content.gsub('{{VERSION}}', version)
    File.write(package_json, generated_content)
  end

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

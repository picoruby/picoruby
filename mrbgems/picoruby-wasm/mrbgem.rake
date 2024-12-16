MRuby::Gem::Specification.new('picoruby-wasm') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'PicoRuby for WebAssembly'

  bin_dir = File.join(build.build_dir, 'bin')
  picoruby_js = File.join(bin_dir, 'picoruby.js')
  custom_js = File.join(dir, 'custom.js')

  directory bin_dir

  file picoruby_js => [File.join(build.build_dir, 'lib', 'libmruby.a'), bin_dir] do |t|
    sh <<~CMD
      emcc -s WASM=1 \
      -s EXPORT_ES6=1 \
      -s MODULARIZE=1 \
      -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap'] \
      -s EXPORTED_FUNCTIONS='["_picorb_init", "_picorb_create_task", "_wasm_tick", "_wasm_run_step"]' \
      -s INITIAL_MEMORY=16MB \
      -s ALLOW_MEMORY_GROWTH=1 \
      -s ENVIRONMENT=web \
      -s WASM_ASYNC_COMPILATION=1 \
      --no-entry \
      --compress-debug-sections \
      #{t.prerequisites.first} \
      -o #{t.name}
    CMD

    wasm = Pathname(picoruby_js).sub_ext('.wasm')
    sh "brotli -f -q 11 #{wasm} -o #{wasm}.br"
    sh "gzip -9 -f #{wasm}"
  end

  build.bins << 'picoruby.js'
end



MRuby::Gem::Specification.new('picoruby-wasm') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'PicoRuby for WebAssembly'
  spec.add_dependency 'picoruby-json'
  spec.add_dependency 'picoruby-dir'
  spec.require_name = 'js'

  bin_dir = File.join(build.build_dir, 'bin')
  picoruby_js = File.join(bin_dir, 'picoruby.js')

  directory bin_dir

  file picoruby_js => [File.join(build.build_dir, 'lib', 'libmruby.a'), bin_dir] do |t|
    sh <<~CMD
      emcc -s WASM=1 \
      -s EXPORT_ES6=1 \
      -s MODULARIZE=1 \
      -s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap", "UTF8ToString", "stringToUTF8", "lengthBytesUTF8"]' \
      -s EXPORTED_FUNCTIONS='["_picorb_init", "_picorb_create_task", "_mrbc_tick", "_mrbc_run_step"]' \
      -s INITIAL_MEMORY=16MB \
      -s ALLOW_MEMORY_GROWTH=1 \
      -s ENVIRONMENT=web \
      -s WASM_ASYNC_COMPILATION=1 \
      -s ASYNCIFY=1 \
      --no-entry \
      --compress-debug-sections \
      #{t.prerequisites.first} \
      -o #{t.name}
    CMD

    picoruby_wasm = Pathname(picoruby_js).sub_ext('.wasm')
    dist_dir = File.join(dir, 'dist')
    sh "cp #{picoruby_js} #{dir}/npm/dist/"
    sh "cp #{picoruby_wasm} #{dir}/npm/dist/"
  end

  build.bins << 'picoruby.js'
end

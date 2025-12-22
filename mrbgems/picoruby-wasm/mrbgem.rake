MRuby::Gem::Specification.new('picoruby-wasm') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'PicoRuby for WebAssembly'

  spec.add_conflict 'picoruby-mrubyc'

  spec.add_dependency 'mruby-compiler2'
  spec.add_dependency 'picoruby-machine'
  spec.add_dependency 'picoruby-jwt'
  spec.add_dependency 'picoruby-sandbox'
  spec.add_dependency 'picoruby-time'

  spec.require_name = 'js'

  # Ensure EM_NODE_JS is set for Emscripten
  unless ENV['EM_NODE_JS']
    node_path = `which node 2>/dev/null`.strip
    if !node_path.empty? && File.executable?(node_path)
      ENV['EM_NODE_JS'] = node_path
      puts "Auto-detected Node.js: #{node_path}"
    end
  end

  bin_dir = File.join(build.build_dir, 'bin')
  output_name = 'picoruby.js'
  output_js = File.join(bin_dir, output_name)

  directory bin_dir

  file output_js => [File.join(build.build_dir, 'lib', 'libmruby.a'), bin_dir] do |t|
    if ENV['PICORUBY_DEBUG']
      optdebug = '-O0 -gsource-map --source-map-base http://127.0.0.1:8080/'
      exported_funcs = '["_picorb_init", "_picorb_create_task", "_picorb_create_task_from_mrb", "_mrb_tick_wasm", "_mrb_run_step", "_malloc", "_free", "_mrb_get_globals_json", "_mrb_eval_string", "_mrb_get_component_debug_info", "_mrb_get_component_state_by_id"]'
    else
      optdebug = '-g0 -O2'
      exported_funcs = '["_picorb_init", "_picorb_create_task", "_picorb_create_task_from_mrb", "_mrb_tick_wasm", "_mrb_run_step", "_malloc", "_free"]'
    end
    sh <<~CMD
      emcc #{optdebug} \
      -s WASM=1 \
      -s EXPORT_ES6=1 \
      -s MODULARIZE=1 \
      -s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap", "UTF8ToString", "stringToUTF8", "lengthBytesUTF8", "HEAPU8"]' \
      -s EXPORTED_FUNCTIONS='#{exported_funcs}' \
      -s INITIAL_MEMORY=16MB \
      -s ALLOW_MEMORY_GROWTH=1 \
      -s ENVIRONMENT=web \
      -s WASM_ASYNC_COMPILATION=1 \
      -s ERROR_ON_UNDEFINED_SYMBOLS=0 \
      --no-entry \
      --compress-debug-sections \
      #{t.prerequisites.first} \
      -o #{t.name}
    CMD

    output_wasm = Pathname(output_js).sub_ext('.wasm')
    output_wasm_map = Pathname(output_wasm).sub_ext('.wasm.map')
    npm_dir = 'npm'
    dist_dir = File.join(dir, npm_dir, 'dist')
    FileUtils.mkdir_p(dist_dir)
    sh "cp #{output_js} #{dist_dir}/"
    sh "cp #{output_wasm} #{dist_dir}/"
    if File.exist?(output_wasm_map)
      sh "cp #{output_wasm_map} #{dist_dir}/"
    end
    sh "brotli -f #{dist_dir}/#{output_wasm.basename}"
  end

  build.bins << output_name
end

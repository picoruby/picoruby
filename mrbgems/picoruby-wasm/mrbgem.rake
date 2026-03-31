MRuby::Gem::Specification.new('picoruby-wasm') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'PicoRuby for WebAssembly'

  spec.add_conflict 'picoruby-mrubyc'
  spec.add_conflict 'picoruby-regexp_light'

  spec.add_dependency 'mruby-compiler2'
  spec.add_dependency 'picoruby-machine'
  spec.add_dependency 'picoruby-jwt'
  spec.add_dependency 'picoruby-picorubyvm'
  spec.add_dependency 'picoruby-sandbox'
  spec.add_dependency 'picoruby-time'

  spec.require_name = 'js'

  spec.cc.defines << "MRB_32BIT"
  spec.cc.defines << "MRB_INT64"
  spec.cc.defines << "MRB_NO_BOXING"
  spec.cc.defines << "MRB_UTF8_STRING"

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
    if ENV['PICORB_DEBUG']
      server_ip = ENV['PICORB_DEBUG_SERVER_IP'] || '127.0.0.1'
      optdebug = "-O0 -gsource-map --source-map-base http://#{server_ip}:8080/"
      exported_funcs = '["_picorb_init", "_picorb_create_task", "_picorb_create_task_from_mrb", "_mrb_tick_wasm", "_mrb_run_step", "_malloc", "_free", "_mrb_get_globals_json", "_mrb_eval_string", "_mrb_get_component_debug_info", "_mrb_get_component_state_by_id", "_mrb_debug_get_status", "_mrb_debug_continue", "_mrb_debug_get_locals", "_mrb_debug_eval_in_binding", "_mrb_debug_step", "_mrb_debug_next", "_mrb_debug_get_callstack"]'
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
      -s INITIAL_MEMORY=32MB \
      -s ALLOW_MEMORY_GROWTH=1 \
      -s STACK_SIZE=2MB \
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
    if ENV['PICORB_DEBUG']
      dist_dir = File.join(dir, 'npm', 'picoruby', 'debug')
    else
      dist_dir = File.join(dir, 'npm', 'picoruby', 'dist')
    end
    FileUtils.mkdir_p(dist_dir)
    sh "cp #{output_js} #{dist_dir}/"
    sh "cp #{output_wasm} #{dist_dir}/"
    if ENV['PICORB_DEBUG'] && File.exist?(output_wasm_map)
      sh "cp #{output_wasm_map} #{dist_dir}/"
    end
    sh "brotli -f #{dist_dir}/#{output_wasm.basename}"
  end

  build.bins << output_name
end

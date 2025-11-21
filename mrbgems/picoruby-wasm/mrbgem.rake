MRuby::Gem::Specification.new('picoruby-wasm') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'PicoRuby for WebAssembly'
  spec.add_dependency 'mruby-compiler2'

  spec.add_dependency 'picoruby-json'
  if build.vm_mrubyc?
    spec.add_dependency 'picoruby-dir'
  end
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
  output_name = build.vm_mrubyc? ? 'picoruby.js' : 'microruby.js'
  output_js = File.join(bin_dir, output_name)

  directory bin_dir

  file output_js => [File.join(build.build_dir, 'lib', 'libmruby.a'), bin_dir] do |t|
    optdebug = ENV['NDEBUG'] ? '-g0' : '-gsource-map --source-map-base http://127.0.0.1:8080/'
    exported_funcs = if build.vm_mrubyc?
      '["_picorb_init", "_picorb_create_task", "_mrbc_tick", "_mrbc_run_step"]'
    else
      '["_picorb_init", "_picorb_create_task", "_mrb_tick_wasm", "_mrb_run_step"]'
    end
    sh <<~CMD
      emcc #{optdebug} \
      -s WASM=1 \
      -s EXPORT_ES6=1 \
      -s MODULARIZE=1 \
      -s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap", "UTF8ToString", "stringToUTF8", "lengthBytesUTF8"]' \
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
    npm_dir = build.vm_mrubyc? ? 'npm-picoruby' : 'npm-microruby'
    dist_dir = File.join(dir, npm_dir, 'dist')
    FileUtils.mkdir_p(dist_dir)
    sh "cp #{output_js} #{dist_dir}/"
    sh "cp #{output_wasm} #{dist_dir}/"
    sh "brotli -f #{dist_dir}/#{output_wasm.basename}"
  end

  build.bins << output_name
end

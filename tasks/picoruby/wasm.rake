namespace :wasm do
  picorbc_npm_dir = "mrbgems/picoruby-wasm/npm/picorbc"

  desc "Build picorbc WASM"
  task :picorbc do
    sh "CONFIG=picorbc-wasm PICORB_DEBUG=1 rake"
    FileUtils.mkdir_p("#{picorbc_npm_dir}/debug")
    sh "cp build/picorbc-wasm/bin/picorbc.js #{picorbc_npm_dir}/debug/"
    sh "cp build/picorbc-wasm/bin/picorbc.wasm #{picorbc_npm_dir}/debug/"
  end

  desc "Build PicoRuby WASM and picorbc WASM (debug)"
  task :debug => :picorbc do
    sh "CONFIG=picoruby-wasm PICORB_DEBUG=1 rake"
  end

  desc "Build PicoRuby WASM and picorbc WASM (production)"
  task :prod do
    sh "CONFIG=picoruby-wasm rake"
  end

  desc "Clean PicoRuby WASM build"
  task :clean do
    sh "CONFIG=picoruby-wasm rake clean"
    sh "CONFIG=picorbc-wasm rake clean"
  end

  desc "Login to npm (if not already logged in)"
  task :npm_login do
    sh "npm login --registry=https://registry.npmjs.org/"
  end

  desc "Check if logged in to npm"
  task :npm_whoami do
    sh "npm whoami --registry=https://registry.npmjs.org/"
  rescue RuntimeError
    abort "Not logged in to npm. Run `rake wasm:npm_login` first."
  end

  desc "Build production and publish to npm"
  task :release => [:npm_whoami, :clean, :prod] do
    FileUtils.cd "mrbgems/picoruby-wasm/npm/picoruby" do
      sh "npm publish --access public --registry=https://registry.npmjs.org/"
    end
    FileUtils.cd "mrbgems/picoruby-wasm/npm/picorbc" do
      sh "npm publish --access public --registry=https://registry.npmjs.org/"
    end
  end

  desc "Build production and publish @picoruby/wasm-wasi to npm with 'head' dist-tag"
  task :release_head => [:npm_whoami, :clean, :prod] do
    require 'json'
    date_suffix = Time.now.strftime('%Y%m%d')
    base_version = File.read("include/version.h").match(/#define PICORUBY_VERSION "(.+?)"/)[1]
    head_version = "#{base_version}-head.#{date_suffix}"
    FileUtils.cd "mrbgems/picoruby-wasm/npm/picoruby" do
      pkg = JSON.parse(File.read("package.json"))
      pkg["version"] = head_version
      File.write("package.json", JSON.generate(pkg))
      sh "npm publish --access public --tag head --registry=https://registry.npmjs.org/"
    end
  end

  desc "Start local server for PicoRuby WASM"
  task :server do
    sh "./mrbgems/picoruby-wasm/demo/bin/server.rb"
  end

  desc "Build debug and publish @picoruby/wasm-wasi to npm with 'debug' dist-tag"
  task :release_debug => [:npm_whoami, :clean] do
    require 'json'
    sh "CONFIG=picoruby-wasm PICORB_DEBUG=1 rake"
    date_suffix = Time.now.strftime('%Y%m%d')
    base_version = File.read("include/version.h").match(/#define PICORUBY_VERSION "(.+?)"/)[1]
    debug_version = "#{base_version}-debug.#{date_suffix}"
    FileUtils.cd "mrbgems/picoruby-wasm/npm/picoruby" do
      pkg = JSON.parse(File.read("package.json"))
      pkg["version"] = debug_version
      File.write("package.json", JSON.generate(pkg))
      sh "npm publish --access public --tag debug --registry=https://registry.npmjs.org/"
    end
  end

  desc "Create ZIP package of the Chrome extension for Web Store submission"
  task :zip_debugger do
    require 'json'
    manifest = JSON.parse(File.read("mrbgems/picoruby-wasm/debugger/manifest.json"))
    version = manifest["version"]
    output = File.expand_path("picoruby-debugger-#{version}.zip")
    FileUtils.rm_f(output)
    FileUtils.cd "mrbgems/picoruby-wasm/debugger" do
      sh "zip -r #{output} . " \
         "-x './README.md' " \
         "-x './picoruby-wasm-privacy-policy.html'"
    end
    puts "Created: picoruby-debugger-#{version}.zip"
  end

  desc "Check PicoRuby WASM npm versions"
  task :versions do
    print "PicoRuby WASM versions:"
    sh "npm view @picoruby/wasm-wasi versions"
    print "picorbc WASM versions:"
    sh "npm view @picoruby/picorbc versions"
  end
end

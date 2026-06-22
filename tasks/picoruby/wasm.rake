namespace :wasm do
  picoruby_npm_dir = "mrbgems/picoruby-wasm/npm/picoruby"
  picorbc_npm_dir = "mrbgems/picoruby-wasm/npm/picorbc"
  npm_registry = "https://registry.npmjs.org/"

  picoruby_version = lambda do
    File.read("include/version.h").match(/#define PICORUBY_VERSION "(.+?)"/)[1]
  end

  publish_picoruby_npm = lambda do |version, tag, source_variant|
    require 'json'
    require 'tmpdir'

    Dir.mktmpdir("picoruby-wasm-npm-") do |tmp_dir|
      package_dir = File.join(tmp_dir, "picoruby")
      FileUtils.cp_r(picoruby_npm_dir, package_dir)

      unless source_variant == "dist"
        source_dir = File.join(picoruby_npm_dir, source_variant)
        dist_dir = File.join(package_dir, "dist")
        FileUtils.rm_rf(dist_dir)
        FileUtils.mkdir_p(dist_dir)
        %w[init.iife.js picoruby.js picoruby.wasm].each do |fname|
          FileUtils.copy_file(File.join(source_dir, fname), File.join(dist_dir, fname))
        end
      end

      package_json_path = File.join(package_dir, "package.json")
      pkg = JSON.parse(File.read(package_json_path))
      pkg["version"] = version
      File.write(package_json_path, JSON.generate(pkg))

      FileUtils.cd package_dir do
        sh "npm publish --access public --tag #{tag} --registry=#{npm_registry}"
      end
    end
  end

  publish_picorbc_npm = lambda do |version|
    require 'json'
    require 'tmpdir'

    Dir.mktmpdir("picorbc-npm-") do |tmp_dir|
      package_dir = File.join(tmp_dir, "picorbc")
      FileUtils.cp_r(picorbc_npm_dir, package_dir)

      package_json_path = File.join(package_dir, "package.json")
      pkg = JSON.parse(File.read(package_json_path))
      pkg["version"] = version
      File.write(package_json_path, JSON.generate(pkg))

      FileUtils.cd package_dir do
        sh "npm publish --access public --registry=#{npm_registry}"
      end
    end
  end

  desc "Build picorbc WASM"
  task :build_picorbc do
    sh "CONFIG=picorbc-wasm PICORB_DEBUG=1 rake"
    FileUtils.mkdir_p("#{picorbc_npm_dir}/debug")
    FileUtils.mkdir_p("#{picorbc_npm_dir}/dist")
    sh "cp build/picorbc-wasm/bin/picorbc.js #{picorbc_npm_dir}/debug/"
    sh "cp build/picorbc-wasm/bin/picorbc.wasm #{picorbc_npm_dir}/debug/"
    sh "cp build/picorbc-wasm/bin/picorbc.js #{picorbc_npm_dir}/dist/"
    sh "cp build/picorbc-wasm/bin/picorbc.wasm #{picorbc_npm_dir}/dist/"
  end

  desc "Build debug PicoRuby WASM into npm/picoruby/debug and picorbc WASM"
  task :build_debug => :build_picorbc do
    sh "CONFIG=picoruby-wasm PICORB_DEBUG=1 rake"
  end

  desc "Build production PicoRuby WASM into npm/picoruby/dist"
  task :build_prod do
    sh "CONFIG=picoruby-wasm rake"
  end

  desc "Build all WASM artifacts required by Funicular release"
  task :build_release_artifacts do
    sh "CONFIG=picorbc-wasm rake clean"
    Rake::Task["wasm:build_picorbc"].invoke

    sh "CONFIG=picoruby-wasm rake clean"
    sh "CONFIG=picoruby-wasm rake"

    sh "CONFIG=picoruby-wasm rake clean"
    sh "CONFIG=picoruby-wasm PICORB_DEBUG=1 rake"

    sh "MRUBY_CONFIG=picoruby-wasm-test rake all"
  end

  desc "Clean PicoRuby WASM build"
  task :clean do
    sh "CONFIG=picoruby-wasm rake clean"
    sh "CONFIG=picorbc-wasm rake clean"
  end

  namespace :npm do
    desc "Login to npm (if not already logged in)"
    task :login do
      sh "npm login --registry=#{npm_registry}"
    end

    desc "Check if logged in to npm"
    task :whoami do
      sh "npm whoami --registry=#{npm_registry}"
    rescue RuntimeError
      abort "Not logged in to npm. Run `rake wasm:npm:login` first."
    end

    desc "Build and publish versioned production and debug npm packages"
    task :publish => :whoami do
      version = picoruby_version.call
      debug_version = "#{version}-debug"

      sh "CONFIG=picorbc-wasm rake clean"
      Rake::Task["wasm:picorbc"].invoke
      publish_picorbc_npm.call(version)

      sh "CONFIG=picoruby-wasm rake clean"
      sh "CONFIG=picoruby-wasm rake"
      publish_picoruby_npm.call(version, "latest", "dist")

      sh "CONFIG=picoruby-wasm rake clean"
      sh "CONFIG=picoruby-wasm PICORB_DEBUG=1 rake"
      publish_picoruby_npm.call(debug_version, "debug", "debug")
    end

    desc "Build and publish head production and debug npm packages"
    task :publish_head => :whoami do
      date_suffix = Time.now.strftime('%Y%m%d')
      base_version = picoruby_version.call
      head_version = "#{base_version}-head.#{date_suffix}"
      debug_version = "#{base_version}-head-debug.#{date_suffix}"

      sh "CONFIG=picoruby-wasm rake clean"
      sh "CONFIG=picoruby-wasm rake"
      publish_picoruby_npm.call(head_version, "head", "dist")

      sh "CONFIG=picoruby-wasm rake clean"
      sh "CONFIG=picoruby-wasm PICORB_DEBUG=1 rake"
      publish_picoruby_npm.call(debug_version, "head-debug", "debug")
    end

    desc "Check PicoRuby WASM npm versions"
    task :versions do
      print "PicoRuby WASM versions:"
      sh "npm view @picoruby/wasm-wasi versions"
      print "picorbc WASM versions:"
      sh "npm view @picoruby/picorbc versions"
    end
  end

  task :npm_login => "wasm:npm:login"
  task :npm_whoami => "wasm:npm:whoami"
  task :release => "wasm:npm:publish"
  task :build_release_head => "wasm:npm:publish_head"
  task :versions => "wasm:npm:versions"

  desc "Start local server for PicoRuby WASM"
  task :server do
    demo_dir = "mrbgems/picoruby-wasm/demo"
    sh "./bin/picorbc -o #{demo_dir}/www/ruby/app.mrb #{demo_dir}/www/ruby/tutorial_helper.rb #{demo_dir}/www/ruby/tutorial_main.rb"
    sh "./#{demo_dir}/bin/server.rb"
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

end

namespace :picorbc do
  desc "create picorbc debug build"
  task :debug do
    sh "MRUBY_CONFIG=picorbc PICORB_DEBUG=1 rake"
  end

  desc "create picorbc production build"
  task :prod do
    sh "MRUBY_CONFIG=picorbc rake clean"
    sh "MRUBY_CONFIG=picorbc rake"
  end
end

namespace :picoruby do
  desc "create production build"
  task :prod do
    sh 'MRUBY_CONFIG=default rake'
  end

  desc "create debug build"
  task :debug do
    sh 'PICORB_DEBUG=1 MRUBY_CONFIG=default rake'
  end

  desc "clean build files"
  task :clean do
    sh "MRUBY_CONFIG=picoruby rake clean"
  end
end

namespace :microruby do
  desc "create production build"
  task :prod do
    sh 'MRUBY_CONFIG=microruby rake'
  end

  desc "create debug build"
  task :debug do
    sh 'PICORB_DEBUG=1 MRUBY_CONFIG=microruby rake'
  end

  desc "clean build files"
  task :clean do
    sh "MRUBY_CONFIG=microruby rake clean"
  end
end

namespace :wasm do
  picorbc_npm_dir = "mrbgems/picoruby-wasm/npm/picorbc"

  desc "Build PicoRuby WASM and picorbc WASM (debug)"
  task :debug do
    sh "CONFIG=picoruby-wasm PICORB_DEBUG=1 rake"
    sh "CONFIG=picorbc-wasm PICORB_DEBUG=1 rake"
    FileUtils.mkdir_p("#{picorbc_npm_dir}/debug")
    sh "cp build/picorbc-wasm/bin/picorbc.js #{picorbc_npm_dir}/debug/"
    sh "cp build/picorbc-wasm/bin/picorbc.wasm #{picorbc_npm_dir}/debug/"
  end

  desc "Build PicoRuby WASM and picorbc WASM (production)"
  task :prod do
    sh "CONFIG=picoruby-wasm rake"
    sh "CONFIG=picorbc-wasm rake"
    FileUtils.mkdir_p("#{picorbc_npm_dir}/dist")
    sh "cp build/picorbc-wasm/bin/picorbc.js #{picorbc_npm_dir}/dist/"
    sh "cp build/picorbc-wasm/bin/picorbc.wasm #{picorbc_npm_dir}/dist/"
  end

  desc "Clean PicoRuby WASM build"
  task :clean do
    sh "CONFIG=picoruby-wasm rake clean"
    sh "CONFIG=picorbc-wasm rake clean"
  end

  task :check_npm_login do
    sh "npm whoami --registry=https://registry.npmjs.org/"
  rescue RuntimeError
    abort "Not logged in to npm. Run `npm login` first."
  end

  desc "Build production and publish to npm"
  task :release => [:check_npm_login, :clean, :prod] do
    FileUtils.cd "mrbgems/picoruby-wasm/npm/picoruby" do
      sh "npm publish --access public --registry=https://registry.npmjs.org/"
    end
    FileUtils.cd "mrbgems/picoruby-wasm/npm/picorbc" do
      sh "npm publish --access public --registry=https://registry.npmjs.org/"
    end
  end

  desc "Start local server for PicoRuby WASM"
  task :server do
    sh "./mrbgems/picoruby-wasm/demo/bin/server.rb"
  end

  desc "Check PicoRuby WASM npm versions"
  task :versions do
    print "PicoRuby WASM versions:"
    sh "npm view @picoruby/wasm-wasi versions"
    print "picorbc WASM versions:"
    sh "npm view @picoruby/picorbc versions"
  end
end


namespace :picorbc do
  desc "create picorbc debug build"
  task :debug do
    sh "MRUBY_CONFIG=picorbc PICORUBY_DEBUG=1 rake clean"
    sh "MRUBY_CONFIG=picorbc PICORUBY_DEBUG=1 rake"
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
    sh 'PICORUBY_DEBUG=1 MRUBY_CONFIG=default rake'
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
    sh 'PICORUBY_DEBUG=1 MRUBY_CONFIG=microruby rake'
  end

  desc "clean build files"
  task :clean do
    sh "MRUBY_CONFIG=microruby rake clean"
  end
end

namespace :wasm do
  desc "Build PicroRuby WASM (mruby VM)"
  task :debug do
    sh "CONFIG=picoruby-wasm PICORUBY_DEBUG=1 rake"
  end

  desc "Build PicoRuby WASM production"
  task :prod do
    sh "CONFIG=picoruby-wasm rake"
  end

  desc "Clean PicoRuby WASM build"
  task :clean do
    sh "CONFIG=picoruby-wasm rake clean"
  end

  desc "Build production and publish it to npm"
  task :release => :prod do
    FileUtils.cd "mrbgems/picoruby-wasm/npm" do
      sh "npm install"
      sh "npm publish --access public"
    end
  end

  desc "Start local server for PicoRuby WASM"
  task :server do
    sh "./mrbgems/picoruby-wasm/demo/bin/server.rb"
  end

  desc "Check PicoRuby WASM npm versions"
  task :versions do
    sh "npm view @picoruby/wasm versions"
  end

  desc "Check versions (PicoRuby)"
  task :versions => 'picoruby:versions'
end


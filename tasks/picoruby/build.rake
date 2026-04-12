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

namespace :femtoruby do
  desc "create production build"
  task :prod do
    sh 'MRUBY_CONFIG=femtoruby rake'
  end

  desc "create debug build"
  task :debug do
    sh 'PICORB_DEBUG=1 MRUBY_CONFIG=femtoruby rake'
  end

  desc "clean build files"
  task :clean do
    sh "MRUBY_CONFIG=femtoruby rake clean"
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
    sh "MRUBY_CONFIG=default rake clean"
  end
end

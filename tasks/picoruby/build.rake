namespace :mrbc do
  desc "create mrbc debug build"
  task :debug do
    sh "MRUBY_CONFIG=mrbc PICORB_DEBUG=1 rake"
  end

  desc "create mrbc production build"
  task :prod do
    sh "MRUBY_CONFIG=mrbc rake clean"
    sh "MRUBY_CONFIG=mrbc rake"
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

namespace :test do

  desc "run all tests"
  task :all => ["compiler:picoruby", "compiler:microruby", "gems:steep"]

  task :build_picoruby_test do
    puts "Building test runner with default.rb..."
    sh "PICORUBY_DEBUG=yes MRUBY_CONFIG=default rake clean"
    sh "PICORUBY_DEBUG=yes MRUBY_CONFIG=default rake all"
  end

  namespace :compiler do
    desc "run compiler tests with mruby VM"
    task :microruby => :picorbc do
      ENV['USE_MRUBY'] = "yes"
      ENV['PICORBC_COMMAND'] = picorbcfile
      ENV['MRUBY_COMMAND'] ||= `RBENV_VERSION=mruby-3.4.0 rbenv which mruby`.chomp
      if ENV['MRUBY_COMMAND'] && ENV['MRUBY_COMMAND'] != ""
        sh "mrbgems/mruby-compiler2/compiler_test/helper/test.rb"
      else
        puts "[WARN] test_compiler_with_mrubyVM skipped because no mruby found"
      end
    end

    desc "run compiler tests with mruby/c VM"
    task :picoruby => :build_picoruby_test do
      ENV['MRUBY_COMMAND'] = picorubyfile
      sh "mrbgems/mruby-compiler2/compiler_test/helper/test.rb"
      ENV['MRUBY_COMMAND'] = nil
    end
  end

  namespace :gems do
    desc "steep check"
    task :steep do
      sh "bundle exec steep check"
    end
  end
end

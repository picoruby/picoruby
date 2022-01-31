# encoding: utf-8
# Build description.
# basic build file for mruby
MRUBY_ROOT = File.dirname(File.expand_path(__FILE__))
MRUBY_BUILD_HOST_IS_CYGWIN = RUBY_PLATFORM.include?('cygwin')
MRUBY_BUILD_HOST_IS_OPENBSD = RUBY_PLATFORM.include?('openbsd')

Rake.verbose(false) if Rake.verbose == Rake::DSL::DEFAULT

$LOAD_PATH << File.join(MRUBY_ROOT, "lib")

# load build systems
require "mruby/core_ext"
require "mruby/build"

# load configuration file
MRUBY_CONFIG = MRuby::Build.mruby_config_path
load MRUBY_CONFIG

# load basic rules
MRuby.each_target do |build|
  build.define_rules
end

# load custom rules
load "#{MRUBY_ROOT}/tasks/core.rake"
load "#{MRUBY_ROOT}/tasks/mrblib.rake"
load "#{MRUBY_ROOT}/tasks/mrbgems.rake"
load "#{MRUBY_ROOT}/tasks/libmruby.rake"
load "#{MRUBY_ROOT}/tasks/bin.rake"
load "#{MRUBY_ROOT}/tasks/presym.rake"
#load "#{MRUBY_ROOT}/tasks/test.rake"
load "#{MRUBY_ROOT}/tasks/benchmark.rake"
load "#{MRUBY_ROOT}/tasks/gitlab.rake"
load "#{MRUBY_ROOT}/tasks/doc.rake"

##############################
# generic build targets, rules
task :default => :all

desc "build all targets, install (locally) in-repo"
task :all => :gensym do
  Rake::Task[:build].invoke
  puts
  puts "Build summary:"
  puts
  MRuby.each_target do |build|
    build.print_build_summary
  end
#  MRuby::Lockfile.write
end

task :build => MRuby.targets.flat_map{|_, build| build.products}

desc "clean all built and in-repo installed artifacts"
task :clean do
  MRuby.each_target do |build|
    rm_rf build.build_dir
    rm_f build.products
  end
  puts "Cleaned up target build folder"
end

desc "clean everything!"
task :deep_clean => %w[clean doc:clean] do
  MRuby.each_target do |build|
    rm_rf build.gem_clone_dir
  end
  puts "Cleaned up mrbgems build folder"
end

##############################
# for PicoRuby

def picorubyfile
  "#{`pwd`.chomp}/bin/picoruby"
end

def picoruby_debug?
  `#{picorubyfile} -v`.include? "debug build"
end

def clean_if(build)
  if File.exist? picorubyfile
    if build == :debug
      if picoruby_debug?
        Rake::Task[:clean].invoke
      end
    elsif !picoruby_debug?
      Rake::Task[:clean].invoke
    end
  end
end

desc "create degub build"
task :debug do
  clean_if(:production)
  sh "PICORUBY_DEBUG=1 CFLAGS='-g3 -O0 -Wall -Wundef -Werror-implicit-function-declaration -Wwrite-strings' rake all"
end

desc "run all tests"
task :test do
  clean_if(:debug)
  Rake::Task[:all].invoke
  sh "PICORUBY=#{picorubyfile} ./test/helper/test.rb"
end

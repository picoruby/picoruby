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
require "picoruby/build"
require "picoruby/runtime_gems"

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
load "#{MRUBY_ROOT}/tasks/picoruby/mrbgems.rake"
load "#{MRUBY_ROOT}/tasks/picoruby/test.rake"
load "#{MRUBY_ROOT}/tasks/libmruby.rake"
load "#{MRUBY_ROOT}/tasks/bin.rake"
load "#{MRUBY_ROOT}/tasks/presym.rake"
#load "#{MRUBY_ROOT}/tasks/test.rake"
load "#{MRUBY_ROOT}/tasks/benchmark.rake"
load "#{MRUBY_ROOT}/tasks/gitlab.rake"
load "#{MRUBY_ROOT}/tasks/doc.rake"
load "#{MRUBY_ROOT}/tasks/r2p2.rake" if File.exist?("#{MRUBY_ROOT}/tasks/r2p2.rake")

task :runtime_gems => RuntimeGems.export(
  output_dir: "#{MRUBY_ROOT}/runtime_gems",
)

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
end

##############################
# for PicoRuby

def picorubyfile
  "#{`pwd`.chomp}/bin/picoruby"
end

def microrubyfile
  "#{`pwd`.chomp}/bin/microruby"
end

def picorbcfile
  "#{`pwd`.chomp}/bin/picorbc"
end

def picoruby_debug?
  `#{picorubyfile} -v`.include? "debug build"
end

def picorbc_debug?
  `#{picorbcfile} -v`.include? "debug build"
end

def clean_if(clean)
  if File.exist? picorubyfile
    if clean == :debug && picoruby_debug?
      Rake::Task[:clean].invoke
    elsif !picoruby_debug?
      Rake::Task[:clean].invoke
    end
  elsif File.exist? picorbcfile
    if clean == :debug && picorbc_debug?
      Rake::Task[:clean].invoke
    elsif !picorbc_debug?
      Rake::Task[:clean].invoke
    end
  else
    # clean anyway
    Rake::Task[:clean].invoke
  end
end

desc "create production build"
task :production do
#  clean_if(:debug)
  Rake::Task[:all].invoke
end

desc "create debug build"
task :debug do
#  clean_if(:production)
  ENV['PICORUBY_DEBUG'] = "yes"
  Rake::Task[:all].invoke
end

desc "create picorbc debug build"
task :picorbc do
  sh "MRUBY_CONFIG=picorbc PICORUBY_DEBUG=yes rake clean"
  sh "MRUBY_CONFIG=picorbc PICORUBY_DEBUG=yes rake"
end

namespace :wasm do
  desc "Build PicroRuby WASM (mruby VM)"
  task :debug do
    sh "CONFIG=picoruby-wasm PICORUBY_DEBUG=1 rake"
  end

  desc "Build PicoRuby WASM production"
  task :prod do
    sh "CONFIG=picoruby-wasm rake clean"
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

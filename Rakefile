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
# Customized for PicoRuby. This should be loaded before libmruby.rake
load "#{MRUBY_ROOT}/tasks/picogems.rake"
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
    if Dir.exist? build.gems['mruby-pico-compiler'].dir
      Dir.chdir build.gems['mruby-pico-compiler'].dir do
        sh "rake clean"
      end
    end
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
  FileUtils.cd "mrbgems/picoruby-mrubyc/repos" do
    rm_rf "mrubyc"
  end
end

##############################
# for PicoRuby

def picorubyfile
  "#{`pwd`.chomp}/bin/picoruby"
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
    if clean == :debug && plicorbc_debug?
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
  clean_if(:debug)
  Rake::Task[:all].invoke
end

desc "create debug build"
task :debug do
  clean_if(:production)
  sh %q{PICORUBY_DEBUG_BUILD=yes rake all}
end

desc "run all tests"
task :test => [:test_mrubyc, :test_mruby]

desc "run all tests with mruby VM"
task :test_mruby => :debug do
  ENV['USE_MRUBY'] = "yes"
  ENV['PICORBC_COMMAND'] ||= picorbcfile
  ENV['MRUBY_COMMAND'] ||= `RBENV_VERSION=mruby-3.1.0 rbenv which mruby`.chomp
  sh "./test_picorbc/helper/test.rb"
end

desc "run all tests with mruby/c VM"
task :test_mrubyc => :debug do
  ENV['MRUBY_COMMAND'] = picorubyfile
  sh "./test_picorbc/helper/test.rb"
  ENV['MRUBY_COMMAND'] = nil
end


current_dir = File.dirname(File.expand_path __FILE__)
picorbc_include_dir = "#{current_dir}/build/repos/host/mruby-pico-compiler/include"

desc "create picorbc executable"
task :picorbc => ["#{picorbc_include_dir}/ptr_size.h", "#{picorbc_include_dir}/parse.h"] do
  Rake::Task["#{current_dir}/bin/picorbc"].invoke
end

# encoding: utf-8
# Build description.
# basic build file for mruby
MRUBY_ROOT = File.dirname(File.expand_path(__FILE__))
MRUBY_BUILD_HOST_IS_CYGWIN = RUBY_PLATFORM.include?('cygwin')
MRUBY_BUILD_HOST_IS_OPENBSD = RUBY_PLATFORM.include?('openbsd')

# The mruby build system is loaded directly from the mruby submodule so that a
# submodule bump immediately exercises the new build system instead of waiting
# for a manual copy-sync. Files picoruby overrides live under lib/ and tasks/
# here; lib/ overrides win by load-path order, task overrides are loaded from
# MRUBY_ROOT below.
MRUBY_SUBMODULE = "#{MRUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby"

Rake.verbose(false) if Rake.verbose == Rake::DSL::DEFAULT

$LOAD_PATH << File.join(MRUBY_ROOT, "lib") << File.join(MRUBY_SUBMODULE, "lib")

# load build systems
require "mruby/core_ext"
require "mruby/build"
require "picoruby/build"
require "picoruby/gem"

# Toolchain definitions come from the submodule. Preload them so that
# `conf.toolchain` finds them in the registry and never falls back to
# loading from "#{MRUBY_ROOT}/tasks/toolchains/".
Dir["#{MRUBY_SUBMODULE}/tasks/toolchains/*.rake"].each {|f| load f}

# load configuration file
MRUBY_CONFIG = MRuby::Build.mruby_config_path
load MRUBY_CONFIG

# Give every cross build the `mrbc` it compiles with. The whole config has been
# read, so a `host` declared after a cross build is as visible as one declared
# before it, and no gem has been set up yet, so nothing has asked for `mrbc`.
MRuby.resolve_mrbc_hosts

# define MRB_NO_GEMS and set up all gems (mirrors the submodule's Rakefile)
MRuby.each_target do |build|
  unless enable_gems? && libmruby_enabled?
    compilers.each do |compiler|
      compiler.defines << "MRB_NO_GEMS"
    end
  end
  gems.setup(self) if enable_gems?

  # The config has been read and every gem's mrbgem.rake body has run, so the
  # defines a gem contributes are all in. `build.has_define?` answers from
  # here on, and refuses before.
  build.defines_final!
end

# load basic rules
MRuby.each_target do |build|
  build.define_rules
end

# load custom rules
load "#{MRUBY_SUBMODULE}/tasks/core.rake"
load "#{MRUBY_SUBMODULE}/tasks/mrblib.rake"
load "#{MRUBY_SUBMODULE}/tasks/mrbgems.rake"
load "#{MRUBY_ROOT}/tasks/picoruby/mrbgems.rake"
load "#{MRUBY_ROOT}/tasks/picoruby/test.rake"
load "#{MRUBY_ROOT}/tasks/picoruby/build.rake"
load "#{MRUBY_ROOT}/tasks/picoruby/r2p2.rake"
load "#{MRUBY_ROOT}/tasks/picoruby/rbenv.rake"
load "#{MRUBY_ROOT}/tasks/picoruby/wasm.rake"
load "#{MRUBY_SUBMODULE}/tasks/libmruby.rake"
load "#{MRUBY_SUBMODULE}/tasks/bin.rake"
load "#{MRUBY_SUBMODULE}/tasks/presym.rake"
#load "#{MRUBY_SUBMODULE}/tasks/test.rake"
load "#{MRUBY_SUBMODULE}/tasks/benchmark.rake"
load "#{MRUBY_SUBMODULE}/tasks/doc.rake"

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
  rm_f "#{MRUBY_ROOT}/build/.last_host_build"
  puts "Cleaned up target build folder"
end

desc "clean everything!"
task :deep_clean => %w[clean doc:clean] do
  MRuby.each_target do |build|
    rm_rf build.gem_clone_dir
  end
end

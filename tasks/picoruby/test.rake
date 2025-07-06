require 'tempfile'
require_relative "#{MRUBY_ROOT}/mrbgems/picoruby-picotest/mrblib/picotest.rb"

namespace :test do

  desc "run all tests"
  task :all => ["compiler:picoruby", "compiler:microruby", "gems:steep"]

  task :build_picoruby_test do
    puts "Building test runner with picoruby-test.rb..."
    sh "PICORUBY_DEBUG=yes MRUBY_CONFIG=picoruby-test rake clean"
    sh "PICORUBY_DEBUG=yes MRUBY_CONFIG=picoruby-test rake all"
  end

  task :build_microruby_test do
    puts "Building test runner with microruby-test.rb..."
    sh "PICORUBY_DEBUG=yes MRUBY_CONFIG=microruby-test rake clean"
    sh "PICORUBY_DEBUG=yes MRUBY_CONFIG=microruby-test rake all"
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

    desc "Run test for a gem on PicoRuby"
    task :picoruby, [:gem_name] => :build_picoruby_test do |t, args|
      run_test_for_gem(args[:gem_name], 'picoruby')
    end

    desc "Run test for a gem on MicroRuby"
    task :microruby, [:gem_name] => :build_microruby_test do |t, args|
      run_test_for_gem(args[:gem_name], 'microruby')
    end

  end
end

def run_test_for_gem(gem_name, vm_type)
  raise "gem_name is required" unless gem_name

  gem_dir = File.expand_path("#{MRUBY_ROOT}/mrbgems/#{gem_name}")
  has_c_extension = Dir.exist?("#{gem_dir}/src")
  is_platform_dependent = Dir.exist?("#{gem_dir}/ports")
  mock_dir = "#{gem_dir}/test/mock"
  has_mock = Dir.exist?(mock_dir)

  load_paths = []
  lib_name = gem_name.gsub('picoruby-', '')

  if has_c_extension && !is_platform_dependent
    # Case 1: Full build for gems with C extensions
    puts "Strategy: Full build for '#{gem_name}'"
    config_path = create_temp_build_config("#{vm_type}-test.rb", gem_name)
    begin
      puts "Building test binary for #{gem_name} on #{vm_type}..."
      sh "PICORUBY_DEBUG=1 MRUBY_CONFIG=#{config_path} rake clean"
      sh "PICORUBY_DEBUG=1 MRUBY_CONFIG=#{config_path} rake all"
      ENV['PICORUBY_TEST_TARGET_VM'] = File.expand_path("./build/host/bin/#{vm_type}")
    ensure
      File.unlink(config_path) if config_path && File.exist?(config_path)
    end
  else
    # Case 2 & 3: Use generic runner based on a clean build
    strategy = has_c_extension ? "Mocking" : "Dynamic require"
    puts "Strategy: #{strategy} for '#{gem_name}'"
    config_path = "#{MRUBY_ROOT}/build_config/#{vm_type}-test.rb"
    puts "Building a generic test runner with #{config_path}..."
    sh "PICORUBY_DEBUG=1 MRUBY_CONFIG=#{config_path} rake clean"
    sh "PICORUBY_DEBUG=1 MRUBY_CONFIG=#{config_path} rake all"
    ENV['PICORUBY_TEST_TARGET_VM'] = File.expand_path("./build/host/bin/#{vm_type}")

    if has_mock
      puts "Found mock files in #{mock_dir}"
      load_paths += Dir.glob("#{mock_dir}/**/*.rb")
    end
    # Add mrblib files to load_paths for dynamic loading
    mrblib_dir = "#{gem_dir}/mrblib"
    load_paths += Dir.glob("#{mrblib_dir}/**/*.rb") if Dir.exist?(mrblib_dir)
    lib_name = nil # Not needed as we load files directly
  end

  unless run_picotest_runner(gem_name, lib_name, load_paths)
    exit 1
  end
end

def create_temp_build_config(base_config_name, gem_name)
  config_file = Tempfile.new(['test_config_', '.rb'])
  base_config_path = File.expand_path("#{MRUBY_ROOT}/build_config/#{base_config_name}")
  config_content = File.read(base_config_path)

  injection_point = /conf\.(picoruby|microruby)/
  injection_text = "conf.gem core: '#{gem_name}'\n  "
  config_content.sub!(injection_point, injection_text + '\1')

  config_file.write(config_content)
  config_file.close
  config_file.path
end

def run_picotest_runner(gem_name, lib_name, load_paths = [])
  gem_dir = File.expand_path("#{MRUBY_ROOT}/mrbgems/#{gem_name}")
  test_dir = File.join(gem_dir, 'test')

  unless Dir.exist?(test_dir)
    puts "WARNING: Test directory not found for #{gem_name}. Skipping."
    return true
  end

  puts "Target VM: #{ENV['PICORUBY_TEST_TARGET_VM']}"
  puts "Test directory: #{test_dir}"
  puts "Library to require: #{lib_name || 'none'}"
  puts "Files to load: #{load_paths.join(', ')}" unless load_paths.empty?

  ENV['RUBY'] = ENV['PICORUBY_TEST_TARGET_VM']

  runner = Picotest::Runner.new(test_dir, nil, "/tmp", lib_name, load_paths)
  error_count = runner.run
  return error_count == 0
end

